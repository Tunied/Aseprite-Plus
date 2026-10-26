// Focused headless integration tests for the native drawing pipeline.
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/cli/app_options.h"
#include "app/context.h"
#include "app/doc.h"
#include "app/docs.h"
#include "app/doc_undo.h"
#include "app/file/file.h"
#include "app/pref/preferences.h"
#include "app/site.h"
#include "app/static_editing.h"
#include "app/tools/clone_stamp.h"
#include "app/tools/tool.h"
#include "app/tools/tool_box.h"
#include "app/tools/tool_loop_manager.h"
#include "app/ui/editor/tool_loop_impl.h"
#include "doc/cel.h"
#include "doc/image.h"
#include "doc/image_spec.h"
#include "doc/layer.h"
#include "doc/layer_tilemap.h"
#include "doc/mask.h"
#include "doc/palette.h"
#include "doc/primitives.h"
#include "doc/sprite.h"
#include "doc/tilesets.h"
#include "os/system.h"
#include "render/rasterize.h"

#include <iostream>
#include <cstdlib>
#include <stdexcept>

using namespace app;
using namespace doc;
using namespace app::tools;
int checks = 0;
void check(bool value, const char* message) {
  ++checks;
  if (!value) throw std::runtime_error(message);
}
color_t pixel(LayerImage* layer, int x, int y) {
  auto* cel = layer->cel(0);
  if (!cel || !cel->bounds().contains(gfx::Point(x, y))) return 0;
  return cel->image()->getPixel(x-cel->x(), y-cel->y());
}
LayerImage* layer(Sprite* sprite, const ImageRef& image = {}, gfx::Point pos = {}) {
  auto* result = new LayerImage(sprite);
  sprite->root()->addLayer(result);
  if (image) {
    auto* cel = new Cel(0, image);
    cel->setPosition(pos);
    result->addCel(cel);
  }
  return result;
}
void stroke(Context* ctx, Doc* doc, LayerImage* target,
            gfx::Point start, gfx::Point end, int opacity = 255, bool cancel = false, int size = 1,
            TilesetMode tilesetMode = TilesetMode::Manual) {
  auto* tool = App::instance()->toolBox()->getToolById("clone_stamp");
  Site site; site.document(doc); site.sprite(doc->sprite()); site.layer(target);
  site.tilesetMode(tilesetMode);
  // Exercise the same whole-tile color-bar mode that the UI can supply.
  if (target->isTilemap()) site.tilemapMode(TilemapMode::Tiles);
  ToolLoopParams params;
  params.tool = tool; params.ink = tool->getInk(0); params.controller = tool->getController(0);
  params.brush = BrushRef(new Brush(kSquareBrushType, size, 0));
  params.opacity = opacity;
  params.fg = Color::fromRgb(255,255,255); params.bg = Color::fromRgb(0,0,0);
  std::unique_ptr<ToolLoop> loop(create_tool_loop_for_script(ctx, site, params));
  check(bool(loop), "tool-loop creation");
  if (target->isTilemap()) check(!loop->isTilemapMode(), "clone always paints pixels on tilemaps");
  ToolLoopManager manager(loop.get());
  Pointer p(start, {}, Pointer::Left, Pointer::Type::Mouse, 1.0f);
  manager.prepareLoop(p); manager.pressButton(p);
  Pointer last(end, {}, Pointer::Left, Pointer::Type::Mouse, 1.0f);
  manager.movement(last); manager.releaseButton(last);
  if (cancel) manager.cancel();
  manager.end();
}

ImageRef rendered(LayerImage* layer) {
  return ImageRef(render::rasterize_with_sprite_bounds(layer->cel(0)));
}

void testTilemaps(Context* context) {
  for (auto format : {IMAGE_RGB, IMAGE_GRAYSCALE, IMAGE_INDEXED}) {
    for (auto mode : {TilesetMode::Manual, TilesetMode::Auto, TilesetMode::Stack}) {
      auto* sprite = new Sprite(ImageSpec(format == IMAGE_RGB ? ColorMode::RGB :
        (format == IMAGE_GRAYSCALE ? ColorMode::GRAYSCALE : ColorMode::INDEXED), 16, 10));
      auto* document = new Doc(sprite);
      context->documents().add(document); context->setActiveDocument(document);
      const color_t a = format == IMAGE_RGB ? rgba(255,0,0,255) :
        (format == IMAGE_GRAYSCALE ? graya(80,255) : 1);
      const color_t b = format == IMAGE_RGB ? rgba(0,255,0,255) :
        (format == IMAGE_GRAYSCALE ? graya(160,255) : 2);
      const color_t c = format == IMAGE_RGB ? rgba(0,0,255,255) :
        (format == IMAGE_GRAYSCALE ? graya(240,255) : 3);
      auto* tileset = new Tileset(sprite, Grid(gfx::Size(2,2)), 3);
      ImageRef pattern(Image::create(format,2,2)); pattern->clear(a);
      pattern->putPixel(1,0,b); pattern->putPixel(0,1,c);
      ImageRef solid(Image::create(format,2,2)); solid->clear(c);
      tileset->set(1,pattern); tileset->set(2,solid);
      auto* tileLayer = new LayerTilemap(sprite,sprite->tilesets()->add(tileset));
      sprite->root()->addLayer(tileLayer);
      ImageRef map(Image::create(IMAGE_TILEMAP,4,2)); map->clear(notile);
      map->putPixel(0,0,tile(1,0)); map->putPixel(1,0,tile(1,tile_f_xflip));
      map->putPixel(2,0,tile(2,0)); map->putPixel(3,0,tile(2,0));
      map->putPixel(0,1,tile(1,tile_f_yflip)); map->putPixel(1,1,tile(1,tile_f_dflip));
      auto* cel = new Cel(0,map); cel->setPosition({1,1}); tileLayer->addCel(cel);
      context->setActiveLayer(tileLayer);
      auto& stamp = document->cloneStamp();
      stamp.sample({3,1},tileLayer->id());
      const auto before = cloneStampSnapshot(document,0);
      check(before && before->pixelFormat() == format && before->getPixel(3,1) == b &&
            before->getPixel(1,3) == c && before->getPixel(3,4) == b,
            "tile source rasterizes flips and offset in sprite pixel coordinates");
      for (int sampleMode : {1,2}) {
        const auto composite = cloneStampSnapshot(document,sampleMode);
        check(composite && composite->getPixel(3,1) == b,
              "visible-layer sampling includes tilemap pixels");
      }
      auto* imageTarget = layer(sprite);
      stroke(context,document,imageTarget,{10,6},{10,6});
      check(pixel(imageTarget,10,6) == b, "tilemap source can clone into ordinary empty layer");
      stamp.sample({3,1},tileLayer->id());
      stroke(context,document,tileLayer,{5,1},{5,1},255,false,1,mode);
      auto after = rendered(tileLayer);
      check(after->getPixel(5,1) == b && after->getPixel(6,1) == c,
            "tilemap destination changes only the brushed pixel");
      check(after->getPixel(7,1) == (mode == TilesetMode::Manual ? b : c),
            "Manual updates shared tile; Auto/Stack keep other tile instances intact");
      check(tileLayer->isTilemap() && tileLayer->cel(0)->image()->pixelFormat() == IMAGE_TILEMAP,
            "clone keeps native tilemap layer and tile IDs");
      document->undoHistory()->undo();
      check(count_diff_between_images(rendered(tileLayer).get(),before.get()) == 0 &&
            tileset->size() == 3, "undo restores tilemap pixels and tileset");
      document->undoHistory()->redo();
      check(count_diff_between_images(rendered(tileLayer).get(),after.get()) == 0,
            "redo restores tilemap stroke");
      stamp.sample({10,6},imageTarget->id());
      stroke(context,document,tileLayer,{5,2},{5,2},255,true,1,mode);
      check(count_diff_between_images(rendered(tileLayer).get(),after.get()) == 0,
            "cancel restores tilemap without partial tile edits");
      // Ordinary source to a tilemap, with a brush crossing the selection edge.
      document->mask()->replace(gfx::Rect(5,2,1,1)); document->setMaskVisible(true);
      stamp.sample({10,6},imageTarget->id());
      stroke(context,document,tileLayer,{5,2},{5,2},128,false,3,mode);
      auto masked = rendered(tileLayer);
      check(masked->getPixel(5,2) != after->getPixel(5,2) &&
            masked->getPixel(6,2) == after->getPixel(6,2) &&
            masked->getPixel(5,1) == after->getPixel(5,1),
            "ordinary source paints tilemap with opacity and selection clipping");
      document->setMaskVisible(false);
      // Empty target cel must be created by the native tilemap pixel pipeline.
      if (mode != TilesetMode::Manual) {
        auto* empty = new LayerTilemap(sprite,tileLayer->tilesetIndex());
        sprite->root()->addLayer(empty);
        stamp.sample({10,6},imageTarget->id());
        stroke(context,document,empty,{12,7},{12,7},255,false,1,mode);
        check(empty->cel(0) && empty->cel(0)->image()->pixelFormat() == IMAGE_TILEMAP &&
              rendered(empty)->getPixel(12,7) == b,"clone creates an empty tilemap target cel");
        document->undoHistory()->undo();
        check(!empty->cel(0),"undo removes newly created tilemap cel");
      }
    }
  }
}

void testTilemapFile(Context* context, const char* filename) {
  auto* document = load_document(context,filename);
  check(document != nullptr,"load supplied tilemap file");
  context->setActiveDocument(document);
  LayerTilemap* target = nullptr;
  for (auto* candidate : document->sprite()->allLayers()) {
    if (candidate->isTilemap() && candidate->cel(0)) {
      target = static_cast<LayerTilemap*>(candidate); break;
    }
  }
  check(target != nullptr,"supplied file contains tilemap layer");
  context->setActiveLayer(target);
  const auto before = rendered(target);
  check(before->pixelFormat() == IMAGE_RGB,"supplied fixture uses RGBA pixels");
  gfx::Point source, destination;
  bool foundSource = false, foundDestination = false;
  for (int y = 0; y < before->height() && !foundDestination; ++y) {
    for (int x = 0; x < before->width(); ++x) {
      const auto value = before->getPixel(x,y);
      if (!foundSource && rgba_geta(value) == 255) { source = {x,y}; foundSource = true; }
      else if (foundSource && value != before->getPixel(source.x,source.y)) {
        destination = {x,y}; foundDestination = true; break;
      }
    }
  }
  check(foundSource && foundDestination,"find distinct source/destination in supplied tilemap");
  const auto count = target->tileset()->size();
  document->cloneStamp().sample(source,target->id());
  stroke(context,document,target,destination,destination,255,false,1,TilesetMode::Auto);
  const auto after = rendered(target);
  check(after->getPixel(destination.x,destination.y) == before->getPixel(source.x,source.y) &&
        count_diff_between_images(before.get(),after.get()) == 1,
        "supplied file clones exactly one pixel while retaining tilemap structure");
  document->undoHistory()->undo();
  check(count_diff_between_images(rendered(target).get(),before.get()) == 0 &&
        target->tileset()->size() == count,"supplied file undo restores pixels and tile count");
  document->undoHistory()->redo();
  check(count_diff_between_images(rendered(target).get(),after.get()) == 0,
        "supplied file redo restores cloned pixel");
  const auto* folder = std::getenv("ASEPRITE_USER_FOLDER");
  check(folder != nullptr,"round-trip requires isolated test folder");
  document->setFilename(std::string(folder) + "/clone-tilemap-roundtrip.aseprite");
  check(save_document(context,document) == 0,"save cloned tilemap to test copy");
  auto* reloaded = load_document(context,document->filename());
  check(reloaded != nullptr,"reload cloned tilemap test copy");
  auto* reloadedLayer = static_cast<LayerImage*>(reloaded->sprite()->root()->firstLayer());
  check(reloadedLayer && reloadedLayer->isTilemap() &&
        count_diff_between_images(rendered(reloadedLayer).get(),after.get()) == 0,
        "tilemap pixels survive aseprite save/reload");
  std::cout << "Supplied tilemap file: clone/undo/redo/save/reload passed\n";
}
int app_main(int argc, char** argv) {
  try {
    auto system = os::System::make();
    const char* args[] = {argv[0], "--batch"};
    AppOptions options(2, args);
    App app; check(app.initialize(options) == 0, "batch initialization");
    auto* context = app.context();
    auto* tool = app.toolBox()->getToolById("clone_stamp");
    check(tool && tool->getInk(0)->isEffect() && tool->getInk(0)->isPaint(), "native clone tool registered");
    Preferences::instance().resetToolPreferences(tool);
    auto& pref = Preferences::instance().tool(tool);
    pref.cloneSample(0);
    const color_t red = rgba(255,0,0,255), green = rgba(0,255,0,255), blue = rgba(0,0,255,255);
    auto* sprite = new Sprite(ImageSpec(ColorMode::RGB, 12, 8));
    auto* doc = new Doc(sprite); context->documents().add(doc); context->setActiveDocument(doc);
    ImageRef image(Image::create(IMAGE_RGB,3,1));
    image->putPixel(0,0,red); image->putPixel(1,0,green); image->putPixel(2,0,blue);
    auto* source = layer(sprite,image,{2,1});
    doc->cloneStamp().sample({2,1},source->id());
    auto& stamp = doc->cloneStamp();
    auto* pairedTarget = layer(sprite);
    stamp.requestSample(gfx::Point(8,5));
    stamp.sample({2,1},source->id());
    check(stamp.showSamplePair && stamp.currentDestination == gfx::Point(8,5) &&
          stamp.currentSource == gfx::Point(2,1) && stamp.offset == gfx::Point(-6,-4),
          "Y destination and clicked source define the green/red reference pair and copy offset");
    check(stamp.previewSource({9,5}) == gfx::Point(3,1),
          "hover source follows the reference offset before any painting");
    stroke(context,doc,pairedTarget,{9,5},{9,5});
    check(pixel(pairedTarget,9,5)==green && !stamp.showSamplePair &&
          stamp.currentSource == gfx::Point(3,1),
          "first painted point keeps Y-to-source offset even when it differs from Y");
    stroke(context,doc,pairedTarget,{10,5},{10,5});
    check(pixel(pairedTarget,10,5)==blue,"aligned strokes retain the explicitly selected offset");
    const auto previousOffset = stamp.offset;
    stamp.requestSample(gfx::Point(7,7));
    stamp.cancelSample();
    check(stamp.offsetValid && stamp.offset == previousOffset && stamp.anchor == gfx::Point(2,1),
          "cancelling a new Y selection preserves the existing copy offset");
    stamp.requestSample(gfx::Point(8,6));
    stamp.sample({2,1},source->id());
    check(stamp.previewSource({9,6}) == gfx::Point(3,1),
          "resampling replaces the Y reference offset");
    stroke(context,doc,pairedTarget,{9,6},{9,6});
    check(pixel(pairedTarget,9,6)==green,"first stroke honors the new Y reference");
    stroke(context,doc,pairedTarget,{10,6},{10,6});
    check(pixel(pairedTarget,10,6)==blue,"subsequent strokes retain the new Y offset");
    CloneStampState horizontal;
    horizontal.requestSample(gfx::Point(10,3));
    horizontal.sample({0,3},source->id());
    horizontal.beginStroke({25,6});
    check(horizontal.currentSource == gfx::Point(15,6) && horizontal.offset == gfx::Point(-10,0),
          "source ten pixels left of Y remains ten pixels left of a later brush position");
    stamp.sample({2,1},source->id());
    stamp.requestSample(gfx::Point(8,5));
    check(stamp.pending && stamp.pendingMarker == gfx::Point(8,5),
          "sampling marks the Y position independently of the source");
    stamp.cancelSample();
    check(!stamp.pending && !stamp.pendingMarker && stamp.valid && stamp.anchor == gfx::Point(2,1),
          "cancelling sampling removes the temporary mark and preserves the previous source");
    stamp.requestSample();
    check(stamp.pending && !stamp.pendingMarker,"toolbar sampling has no canvas position marker");
    stamp.sample({2,1},source->id());
    check(!stamp.pending && !stamp.pendingMarker && stamp.previewSource({8,5}) == gfx::Point(2,1),
          "confirmed sampling clears the mark and previews the anchor before the first stroke");
    stroke(context,doc,source,{3,1},{5,1});
    check(stamp.currentDestination == gfx::Point(5,1) && stamp.currentSource == gfx::Point(4,1),
          "guide endpoints match actual drawing coordinates with an offset cel");
    check(stamp.previewSource({8,5}) == gfx::Point(7,5),
          "hover guide tracks the established offset");
    check(pixel(source,3,1)==red && pixel(source,4,1)==green && pixel(source,5,1)==blue,
          "same-layer overlap reads pre-expansion snapshot with cel offset");
    doc->undoHistory()->undo();
    check(pixel(source,3,1)==green && pixel(source,4,1)==blue && pixel(source,5,1)==0,"one-stroke undo");
    doc->undoHistory()->redo();
    check(pixel(source,3,1)==red && pixel(source,5,1)==blue,"one-stroke redo");
    auto* target = layer(sprite);
    doc->cloneStamp().sample({2,1},source->id());
    stroke(context,doc,target,{1,3},{1,3},128);
    check(rgba_getr(pixel(target,1,3))==255 && rgba_geta(pixel(target,1,3))==128,
          "empty target and opacity do not accumulate within a stroke");
    stroke(context,doc,target,{2,3},{2,3});
    check(pixel(target,2,3)==red,"aligned source offset persists between strokes");
    stroke(context,doc,target,{3,3},{4,3});
    check(pixel(target,3,3)==green && pixel(target,4,3)==blue,
          "later strokes continue copying relative to the established destination reference");
    check(stamp.currentDestination == gfx::Point(4,3) && stamp.currentSource == gfx::Point(5,1) &&
          stamp.previewSource({8,5}) == gfx::Point(9,3),
          "guide follows the same relative coordinates during and between strokes");
    stamp.sample({2,1},source->id());
    stroke(context,doc,target,{5,3},{5,3},255,true);
    check(pixel(target,5,3)==0,"cancel rolls back destination");
    stamp.sample({2,1},source->id());
    doc->mask()->replace(gfx::Rect(6,4,1,1)); doc->setMaskVisible(true);
    stroke(context,doc,target,{6,4},{7,4},255,false,3);
    check(pixel(target,6,4)==red && pixel(target,7,4)==0 && pixel(target,6,3)==0,"hard brush obeys selection");
    doc->setMaskVisible(false);
    doc->cloneStamp().sample({0,0},source->id());
    stroke(context,doc,target,{8,5},{8,5});
    check(pixel(target,8,5)==0,"transparent source skips destination");
    doc->cloneStamp().sample({11,7},source->id());
    stroke(context,doc,target,{8,5},{10,5});
    check(pixel(target,10,5)==0,"out-of-bounds source safely skipped");
    // Visible-layer rendering includes group opacity and excludes layers above cutoff.
    auto* group = new LayerGroup(sprite); sprite->root()->addLayer(group);
    group->setOpacity(128);
    ImageRef dot(Image::create(IMAGE_RGB,1,1)); dot->putPixel(0,0,blue);
    auto* grouped = new LayerImage(sprite); group->addLayer(grouped); grouped->addCel(new Cel(0,dot));
    auto* upper = layer(sprite,dot,{0,0});
    doc->cloneStamp().sample({0,0},grouped->id());
    auto below = cloneStampSnapshot(doc,1), all = cloneStampSnapshot(doc,2);
    check(rgba_geta(below->getPixel(0,0))==128 && rgba_geta(all->getPixel(0,0))==255,"below cutoff preserves group opacity");
    upper->setVisible(false);
    all = cloneStampSnapshot(doc,2);
    check(rgba_geta(all->getPixel(0,0))==128,"hidden layers excluded from all-visible snapshot");
    doc->cloneStamp().sample({2,1},source->id());
    sprite->root()->removeLayer(source); delete source;
    check(!cloneStampSnapshot(doc,0) && !doc->cloneStamp().valid,"deleted/invalid source safely invalidated");
    check(isAnimationCommand("NewFrame") && isAnimationCommand("PlayAnimation") &&
          isAnimationCommand("ShowOnionSkin") && !isAnimationCommand("NewLayer") &&
          !isAnimationCommand("Timeline") && !isAnimationCommand("CloneStampSource"),"static UI command filter keeps layer tools");
    // Grayscale and indexed mode use the same actual tool-loop.
    for (auto format : {IMAGE_GRAYSCALE, IMAGE_INDEXED}) {
      auto* s = new Sprite(ImageSpec(format==IMAGE_INDEXED ? ColorMode::INDEXED : ColorMode::GRAYSCALE,8,4));
      auto* d = new Doc(s); context->documents().add(d); context->setActiveDocument(d);
      ImageRef img(Image::create(format,1,1)); const color_t value = format==IMAGE_INDEXED ? 7 : graya(180,255);
      img->putPixel(0,0,value); auto* src = layer(s,img); auto* dst = layer(s);
      d->cloneStamp().sample({0,0},src->id()); pref.cloneSample(0);
      stroke(context,d,dst,{2,1},{2,1});
      check(pixel(dst,2,1)==value,"grayscale/indexed exact pixel clone");
      d->undoHistory()->undo(); check(!dst->cel(0),"empty cel creation undone");
      if (format == IMAGE_INDEXED) {
        ImageRef background(Image::create(IMAGE_INDEXED,8,4)); background->clear(0);
        src->cel(0)->data()->setImage(background,src); src->configureAsBackground();
        s->palette(0)->setEntry(0,rgba(1,2,3,255));
        s->palette(0)->setEntry(5,rgba(1,2,3,255));
        d->cloneStamp().sample({0,0},src->id());
        stroke(context,d,dst,{2,1},{2,1});
        check(pixel(dst,2,1)==5,"opaque background reserved index maps to an opaque target index");
      }
    }
    testTilemaps(context);
    if (argc > 1) testTilemapFile(context,argv[1]);
    std::cout << "Clone stamp integration: " << checks << " checks passed\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << "FAILED: " << e.what() << '\n'; return 1; }
}
