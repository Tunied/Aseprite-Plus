// Headless regression coverage for the real paint and floating-selection pipelines.
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "app/app.h"
#include "app/cli/app_options.h"
#include "app/cmd/clear_mask.h"
#include "app/context.h"
#include "app/doc.h"
#include "app/docs.h"
#include "app/doc_undo.h"
#include "app/pref/preferences.h"
#include "app/site.h"
#include "app/tools/tool.h"
#include "app/tools/tool_box.h"
#include "app/tools/tool_loop_manager.h"
#include "app/tx.h"
#include "app/ui/editor/pixels_movement.h"
#include "app/ui/editor/tool_loop_impl.h"
#include "app/util/new_image_from_mask.h"
#include "app/util/expand_cel_canvas.h"
#include "doc/cel.h"
#include "doc/image.h"
#include "doc/image_spec.h"
#include "doc/layer.h"
#include "doc/layer_tilemap.h"
#include "doc/mask.h"
#include "doc/primitives.h"
#include "doc/sprite.h"
#include "doc/tilesets.h"
#include "os/system.h"
#include "render/rasterize.h"
#include <iostream>
#include <stdexcept>

using namespace app;
using namespace doc;
using namespace app::tools;
namespace {
int checks = 0;
void check(bool value, const char* label)
{
  ++checks;
  if (!value) throw std::runtime_error(label);
}
color_t pixel(LayerImage* layer, int x, int y)
{
  auto* cel = layer->cel(0);
  return cel && cel->bounds().contains(gfx::Point(x,y)) ?
    cel->image()->getPixel(x-cel->x(), y-cel->y()) : 0;
}
struct Fixture {
  Context* context;
  Doc* document;
  LayerImage* layer;
  Site site;
  Fixture(Context* ctx, PixelFormat format = IMAGE_RGB, bool empty = false,
          gfx::Point origin = gfx::Point(0,0), bool tilemap = false) : context(ctx)
  {
    auto* sprite = new Sprite(ImageSpec(format == IMAGE_RGB ? ColorMode::RGB :
      (format == IMAGE_GRAYSCALE ? ColorMode::GRAYSCALE : ColorMode::INDEXED), 12, 8));
    document = new Doc(sprite);
    ctx->documents().add(document);
    ctx->setActiveDocument(document);
    if (tilemap) {
      auto* tileset = new Tileset(sprite, Grid(gfx::Size(2,2)), 2);
      ImageRef tileImage(Image::create(format,2,2));
      tileImage->clear(format == IMAGE_RGB ? rgba(0,255,0,255) :
                       format == IMAGE_GRAYSCALE ? graya(160,255) : 2);
      tileset->set(1,tileImage);
      layer = new LayerTilemap(sprite,sprite->tilesets()->add(tileset));
    }
    else
      layer = new LayerImage(sprite);
    sprite->root()->addLayer(layer);
    if (!empty) {
      ImageRef image(Image::create(tilemap ? IMAGE_TILEMAP : format,
                                   tilemap ? 4 : 12, tilemap ? 2 : 8));
      image->clear(tilemap ? tile(1,0) : 0);
      if (tilemap) image->putPixel(1,0,tile(1,tile_f_xflip));
      auto* cel = new Cel(0,image);
      cel->setPosition(origin);
      layer->addCel(cel);
    }
    ctx->setActiveLayer(layer);
    site.document(document); site.sprite(sprite); site.layer(layer);
    document->gridLock().request();
    check(document->gridLock().select({5,3},{0,0,4,2},sprite->bounds()), "pick grid");
  }
  void select(const gfx::Rect& bounds)
  {
    Mask mask; mask.replace(bounds); document->setMask(&mask);
    document->setMaskVisible(true);
  }
  void stroke(const char* name, gfx::Point a, gfx::Point b, int size = 1, bool cancel = false)
  {
    auto* tool = App::instance()->toolBox()->getToolById(name);
    Preferences::instance().resetToolPreferences(tool);
    ToolLoopParams params;
    params.tool=tool; params.ink=tool->getInk(0); params.controller=tool->getController(0);
    params.brush=BrushRef(new Brush(kSquareBrushType,size,0));
    params.fg=Color::fromRgb(255,0,0); params.bg=Color::fromRgb(0,0,0);
    std::unique_ptr<ToolLoop> loop(create_tool_loop_for_script(context,site,params));
    check(bool(loop), "create paint loop");
    if (layer->isTilemap() && document->gridLock().locked())
      check(!loop->isTilemapMode(), "focused tilemap painting uses pixel coordinates");
    ToolLoopManager manager(loop.get());
    Pointer first(a,{},Pointer::Left,Pointer::Type::Mouse,1.0f);
    Pointer last(b,{},Pointer::Left,Pointer::Type::Mouse,1.0f);
    manager.prepareLoop(first); manager.pressButton(first);
    manager.movement(last); manager.releaseButton(last);
    if (cancel) manager.cancel();
    manager.end();
  }
};
ImageRef rendered(Fixture& f)
{
  return ImageRef(render::rasterize_with_sprite_bounds(f.layer->cel(0)));
}
void tilemapPainting(Context* context)
{
  for (auto format : {IMAGE_RGB,IMAGE_GRAYSCALE,IMAGE_INDEXED}) {
    for (auto tilemapMode : {TilemapMode::Pixels,TilemapMode::Tiles}) {
      for (auto mode : {TilesetMode::Manual,TilesetMode::Auto,TilesetMode::Stack}) {
        for (const char* tool : {"pencil","eraser","paint_bucket","filled_rectangle"}) {
          Fixture f(context,format,false,{1,1},true);
          f.site.tilemapMode(tilemapMode); f.site.tilesetMode(mode);
          auto* target = static_cast<LayerTilemap*>(f.layer);
          // Focus only part of a tile shared by every other cell, including a flipped copy.
          f.document->gridLock().request();
          check(f.document->gridLock().select({5,1},{5,1,1,2},f.document->sprite()->bounds()),
                "focus partial tile with offset cel");
          const auto before = rendered(f);
          const auto tileCount = target->tileset()->size();
          f.select({5,1,2,1});
          f.stroke(tool,{5,1},{6,2},5);
          const auto after = rendered(f);
          check(after->getPixel(5,1) != before->getPixel(5,1) &&
                count_diff_between_images(before.get(),after.get()) == 1,
                "focused pencil/eraser/fill/shape changes only focus and selection intersection");
          check(f.site.tilemapMode() == tilemapMode && f.site.tilesetMode() == mode,
                "focused stroke preserves caller tilemap and tileset modes");
          check(f.document->mask()->bounds() == gfx::Rect(5,1,2,1),
                "focused stroke preserves user selection");
          f.document->undoHistory()->undo();
          check(count_diff_between_images(rendered(f).get(),before.get()) == 0 &&
                target->tileset()->size() == tileCount,
                "focused tilemap undo restores pixels and tileset");
          f.document->gridLock().reset();
          f.document->undoHistory()->redo();
          check(count_diff_between_images(rendered(f).get(),after.get()) == 0,
                "focused tilemap redo survives clearing focus");
          f.document->gridLock().request();
          f.document->gridLock().select({5,1},{5,1,1,2},f.document->sprite()->bounds());
          f.document->setMaskVisible(false);
          f.stroke(tool,{5,2},{5,2},5,true);
          check(count_diff_between_images(rendered(f).get(),after.get()) == 0,
                "cancelling focused tilemap stroke preserves pixels");
          f.stroke(tool,{10,6},{11,7});
          check(count_diff_between_images(rendered(f).get(),after.get()) == 0,
                "tilemap stroke outside focused cell does nothing");
          f.select({0,0,1,1});
          f.stroke(tool,{5,2},{5,2},5);
          check(count_diff_between_images(rendered(f).get(),after.get()) == 0,
                "disjoint selection blocks focused tilemap stroke");
          // Use a fresh document: native no-op tilemap transactions can still
          // carry undo metadata, so do not assume one undo skips all no-ops.
          Fixture full(context,format,false,{1,1},true);
          full.site.tilemapMode(tilemapMode); full.site.tilesetMode(mode);
          full.document->gridLock().request();
          full.document->gridLock().select({5,1},{5,1,1,2},full.document->sprite()->bounds());
          full.stroke(tool,{5,1},{6,2},5);
          const auto fullCell = rendered(full);
          check(count_diff_between_images(before.get(),fullCell.get()) == 2 &&
                fullCell->getPixel(5,1) != before->getPixel(5,1) &&
                fullCell->getPixel(5,2) != before->getPixel(5,2),
                "without selection focused tilemap stroke edits full cell and preserves shared copies");
        }
      }
    }
  }
  // Focus also allows Manual to paint an empty tilemap without changing its preference.
  Fixture empty(context,IMAGE_RGB,true,{},true);
  empty.site.tilemapMode(TilemapMode::Tiles); empty.site.tilesetMode(TilesetMode::Manual);
  empty.stroke("pencil",{5,2},{5,2},5);
  check(empty.layer->cel(0) && rendered(empty)->getPixel(5,2) == rgba(255,0,0,255),
        "focused pencil creates pixels in empty Manual tilemap");
  empty.document->undoHistory()->undo();
  check(!empty.layer->cel(0), "undo removes newly painted tilemap cel");
}
void linkedTilemapPainting(Context* context)
{
  for (auto input : {TilemapMode::Pixels, TilemapMode::Tiles}) {
    for (auto mode : {TilesetMode::Manual, TilesetMode::Auto, TilesetMode::Stack}) {
      for (const char* tool : {"pencil", "eraser", "paint_bucket", "filled_rectangle"}) {
        Fixture f(context,IMAGE_RGB,false,{1,1},true);
        f.layer->userData().setText("dual-grid-optimize-v1");
        auto* tileset = static_cast<LayerTilemap*>(f.layer)->tileset();
        tileset->getTileData(1).setText("dual-grid-index:4");
        f.site.tilemapMode(input); f.site.tilesetMode(mode);
        f.document->gridLock().request();
        f.document->gridLock().select({5,1},{5,1,1,2},f.document->sprite()->bounds());
        f.select({5,1,2,1});
        const auto before = rendered(f);
        ImageRef map(Image::createCopy(f.layer->cel(0)->image()));
        f.stroke(tool,{5,1},{6,2},5);
        const auto after = rendered(f);
        check(after->getPixel(5,1) != before->getPixel(5,1) &&
              after->getPixel(1,1) == after->getPixel(5,1),
              "linked Index edits propagate to shared references");
        check(after->getPixel(5,2) == before->getPixel(5,2),
              "linked painting still clips the source to focus and selection");
        check(tileset->size() == 2 && tileset->getTileData(1).text() == "dual-grid-index:4" &&
              count_diff_between_images(map.get(),f.layer->cel(0)->image()) == 0,
              "linked focus preserves Tile IDs, metadata and tilemap references");
        check(f.site.tilemapMode() == input && f.site.tilesetMode() == mode,
              "linked focus preserves caller mode preferences");
        f.document->undoHistory()->undo();
        check(count_diff_between_images(before.get(),rendered(f).get()) == 0,
              "linked focus undo restores shared pixels");
        f.document->gridLock().reset();
        f.document->undoHistory()->redo();
        check(count_diff_between_images(after.get(),rendered(f).get()) == 0,
              "linked focus redo retains shared result after focus is cleared");
        f.document->gridLock().request();
        f.document->gridLock().select({5,1},{5,1,1,2},f.document->sprite()->bounds());
        f.document->setMaskVisible(false);
        f.stroke(tool,{5,2},{5,2},5,true);
        check(count_diff_between_images(after.get(),rendered(f).get()) == 0,
              "linked focus cancellation preserves all shared pixels");
      }
    }
  }
}
void checkOutside(Fixture& f, const Image* before)
{
  for (int y=0; y<8; ++y)
    for (int x=0; x<12; ++x)
      if (!f.document->gridLock().bounds.contains(gfx::Point(x,y)))
        check(pixel(f.layer,x,y)==before->getPixel(x,y), "outside pixels preserved");
}
void movement(Context* context, bool cut)
{
  Fixture f(context);
  auto* img = f.layer->cel(0)->image();
  for (int y=0; y<8; ++y)
    for (int x=0; x<12; ++x)
      img->putPixel(x,y,rgba(10+x*15,20+y*20,30,255));
  ImageRef before(Image::createCopy(img));
  f.select({0,0,6,4}); // Texture includes cells outside the lock at (4,2,4,2).
  ImageRef source(new_image_from_mask(f.site,f.document->mask(),false));
  check(source && source->width()==6 && source->height()==4, "copy full outside texture");
  {
    PixelsMovement move(context,f.site,source,f.document->mask(),"Grid lock movement");
    if (cut) move.cutMask(); else move.copyMask();
    auto transform = move.getTransformation();
    // Exercise successive horizontal and vertical nudges through the same
    // transformation interface used by MovingPixelsState keyboard handling.
    const gfx::Point steps[]={{1,0},{1,0},{1,0},{1,0},{-1,0},{0,1},{0,1},{0,-1}};
    for (const auto& step : steps) {
      auto bounds=transform.bounds(); bounds.offset(step.x,step.y);
      transform.bounds(bounds); move.setTransformation(transform);
      const auto extra=f.document->extraCel();
      for (int y=0; y<extra->image()->height(); ++y)
        for (int x=0; x<extra->image()->width(); ++x) {
          const auto p=extra->cel()->position()+gfx::Point(x,y);
          if (before->bounds().contains(p) && !f.document->gridLock().bounds.contains(p))
            check(extra->image()->getPixel(x,y)==before->getPixel(p.x,p.y),
                  "floating preview preserves outside pixels");
        }
    }
    std::unique_ptr<Image> dragged;
    std::unique_ptr<Mask> mask;
    move.getDraggedImageCopy(dragged,mask);
    check(dragged->width()==6 && dragged->height()==4, "clipboard keeps full transformed texture");
    move.dropImage();
  }
  checkOutside(f,before.get());
  for(int y=2;y<4;++y)
    for(int x=4;x<8;++x)
      check(pixel(f.layer,x,y)==before->getPixel(x-3,y-1), "texture pasted inside locked cell");
  f.document->gridLock().reset();
  f.document->undoHistory()->undo();
  for(int y=0;y<8;++y) for(int x=0;x<12;++x)
    check(pixel(f.layer,x,y)==before->getPixel(x,y), "movement undo restores entire image");
  f.document->undoHistory()->redo();
  for(int y=0;y<8;++y) for(int x=0;x<12;++x)
    if (!gfx::Rect(4,2,4,2).contains(gfx::Point(x,y)))
      check(pixel(f.layer,x,y)==before->getPixel(x,y), "redo after unlock preserves outside pixels");
}
}
int app_main(int argc, char** argv)
{
  try {
    auto system=os::System::make();
    const char* args[]={argv[0],"--batch"};
    AppOptions options(2,args);
    App app; check(app.initialize(options)==0,"batch initialization");
    auto* ctx=app.context();
    GridLockState lock;
    lock.request();
    check(!lock.select({-1,0},{0,0,4,4},{0,0,12,8}) && lock.pending,"outside pick stays pending");
    check(lock.select({0,0},{2,1,4,4},{0,0,12,8}) && lock.bounds==gfx::Rect(0,0,2,1),
          "offset grid uses floor division and clips edge cell");
    lock.reset(); check(!lock.enabled(),"unlock clears pending and bounds");
    for (auto format : {IMAGE_RGB,IMAGE_GRAYSCALE,IMAGE_INDEXED}) {
      Fixture f(ctx,format);
      ImageRef before(Image::createCopy(f.layer->cel(0)->image()));
      f.stroke("pencil",{0,2},{11,2},3);
      checkOutside(f,before.get());
      check(pixel(f.layer,5,2)!=0,"pencil paints locked cell");
      f.document->undoHistory()->undo();
      check(pixel(f.layer,5,2)==0,"paint undo");
      f.document->undoHistory()->redo();
      f.stroke("eraser",{0,2},{11,2},5);
      checkOutside(f,before.get());
      check(pixel(f.layer,5,2)==0,"eraser clips to lock");
    }
    {
      Fixture f(ctx,IMAGE_RGB,true);
      f.stroke("pencil",{0,0},{11,7},7);
      check(f.layer->cel(0) && gfx::Rect(4,2,4,2).contains(f.layer->cel(0)->bounds()),
            "new cel contains only locked pixels");
      f.document->undoHistory()->undo(); check(!f.layer->cel(0),"new cel undo");
      f.stroke("pencil",{0,0},{11,7},7,true);
      check(!f.layer->cel(0),"cancel leaves empty layer unchanged");
    }
    {
      Fixture f(ctx,IMAGE_RGB,false,{-2,-1});
      f.select({0,0,2,2});
      f.stroke("pencil",{0,0},{11,7},9);
      check(pixel(f.layer,5,2)==0 && f.document->mask()->bounds()==gfx::Rect(0,0,2,2),
            "disjoint selection paints nothing without replacing selection");
      f.select({5,1,2,4});
      f.stroke("pencil",{0,2},{11,2},5);
      check(pixel(f.layer,5,2)!=0 && pixel(f.layer,4,2)==0 && pixel(f.layer,7,2)==0,
            "offset cel intersects lock with user selection");
    }
    {
      Fixture f(ctx);
      f.stroke("paint_bucket",{5,2},{5,2});
      for(int y=0;y<8;++y) for(int x=0;x<12;++x)
        check((pixel(f.layer,x,y)!=0)==gfx::Rect(4,2,4,2).contains(gfx::Point(x,y)),
              "flood fill affects exactly one locked cell");
      f.select({0,0,12,8});
      { Tx tx(f.document,"clear locked cell"); tx(new cmd::ClearMask(f.layer->cel(0))); tx.commit(); }
      check(pixel(f.layer,5,2)==0,"delete clips to lock");
      f.document->undoHistory()->undo(); check(pixel(f.layer,5,2)!=0,"delete undo");
      f.document->gridLock().reset();
      f.document->undoHistory()->redo(); check(pixel(f.layer,5,2)==0,"delete redo after unlock");
    }
    {
      Fixture f(ctx);
      for(int x=0;x<4;++x)
        f.layer->cel(0)->image()->putPixel(x,0,rgba(20+x*30,40,80,255));
      ImageRef before(Image::createCopy(f.layer->cel(0)->image()));
      f.document->cloneStamp().sample({0,0},f.layer->id());
      f.stroke("clone_stamp",{4,2},{11,2});
      checkOutside(f,before.get());
      check(pixel(f.layer,5,2)==before->getPixel(1,0),"clone samples freely outside locked cell");
    }
    {
      Fixture f(ctx);
      f.layer->cel(0)->image()->clear(rgba(90,100,110,255));
      ImageRef before(Image::createCopy(f.layer->cel(0)->image()));
      f.document->setMaskVisible(false);
      { Tx tx(f.document,"clear without selection"); tx(new cmd::ClearMask(f.layer->cel(0))); tx.commit(); }
      checkOutside(f,before.get());
      check(pixel(f.layer,5,2)==0,"delete without selection clears only lock");
      f.document->undoHistory()->undo();
      check(pixel(f.layer,5,2)==before->getPixel(5,2),"clear without selection undo");
      f.document->gridLock().reset();
      f.document->undoHistory()->redo();
      for(int y=0;y<8;++y) for(int x=0;x<12;++x)
        check(pixel(f.layer,x,y)==(gfx::Rect(4,2,4,2).contains(gfx::Point(x,y)) ? 0 : before->getPixel(x,y)),
              "clear without selection redo preserves outside after unlock");
    }
    {
      Fixture f(ctx);
      f.layer->cel(0)->image()->clear(rgba(90,100,110,255));
      ImageRef before(Image::createCopy(f.layer->cel(0)->image()));
      {
        Tx tx(f.document,"expanded canvas paste");
        ExpandCelCanvas expand(f.site,f.layer,filters::TiledMode::NONE,tx,ExpandCelCanvas::None);
        expand.validateDestCanvas(gfx::Region(f.document->sprite()->bounds()));
        expand.getDestCanvas()->clear(rgba(255,0,0,255));
        expand.commit(); tx.commit();
      }
      checkOutside(f,before.get());
      check(pixel(f.layer,5,2)==rgba(255,0,0,255),"expanded canvas commit clips unmasked writes");
    }
    tilemapPainting(ctx);
    linkedTilemapPainting(ctx);
    movement(ctx,false);
    movement(ctx,true);
    std::cout << "Grid lock integration: " << checks << " checks passed\n";
    return 0;
  }
  catch(const std::exception& e) { std::cerr << "FAILED: " << e.what() << '\n'; return 1; }
}
