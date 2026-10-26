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
#include "doc/mask.h"
#include "doc/sprite.h"
#include "os/system.h"
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
          gfx::Point origin = gfx::Point(0,0)) : context(ctx)
  {
    auto* sprite = new Sprite(ImageSpec(format == IMAGE_RGB ? ColorMode::RGB :
      (format == IMAGE_GRAYSCALE ? ColorMode::GRAYSCALE : ColorMode::INDEXED), 12, 8));
    document = new Doc(sprite);
    ctx->documents().add(document);
    ctx->setActiveDocument(document);
    layer = new LayerImage(sprite);
    sprite->root()->addLayer(layer);
    if (!empty) {
      ImageRef image(Image::create(format, 12, 8));
      image->clear(0);
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
    ToolLoopManager manager(loop.get());
    Pointer first(a,{},Pointer::Left,Pointer::Type::Mouse,1.0f);
    Pointer last(b,{},Pointer::Left,Pointer::Type::Mouse,1.0f);
    manager.prepareLoop(first); manager.pressButton(first);
    manager.movement(last); manager.releaseButton(last);
    if (cancel) manager.cancel();
    manager.end();
  }
};
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
    movement(ctx,false);
    movement(ctx,true);
    std::cout << "Grid lock integration: " << checks << " checks passed\n";
    return 0;
  }
  catch(const std::exception& e) { std::cerr << "FAILED: " << e.what() << '\n'; return 1; }
}
