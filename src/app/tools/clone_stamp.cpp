// Aseprite Plus - hard-edged pixel clone stamp.
// Distributed under the End-User License Agreement for Aseprite.
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/tools/clone_stamp.h"

#include "app/doc.h"
#include "app/pref/preferences.h"
#include "app/tools/tool_loop.h"
#include "doc/blend_funcs.h"
#include "doc/image.h"
#include "doc/layer.h"
#include "doc/mask.h"
#include "doc/palette.h"
#include "doc/rgbmap.h"
#include "doc/sprite.h"
#include "render/rasterize.h"
#include "render/render.h"

#include <functional>
#include <set>

namespace app { namespace tools {

doc::ImageRef cloneStampSnapshot(Doc* document, int sampleMode)
{
  auto& state = document->cloneStamp();
  auto* sprite = document->sprite();
  doc::Layer* source = nullptr;
  std::set<const doc::Layer*> below;
  bool found = false;
  std::function<void(doc::Layer*)> visit = [&](doc::Layer* layer) {
    if (layer->id() == state.sourceLayer) {
      source = layer;
      found = true;
      below.insert(layer);
    }
    else if (layer->isGroup()) {
      // Keep ancestors so group opacity/blending still applies.
      if (!found) below.insert(layer);
      for (auto* child : layer->layers()) visit(child);
    }
    else if (!found) below.insert(layer);
  };
  visit(sprite->root());
  if (!state.valid || !source || !source->isImage() || source->isReference()) {
    state.valid = state.offsetValid = false;
    return {};
  }

  // Each stroke reads a fresh immutable snapshot, independent of destination edits.
  if (sampleMode == 0) {
    if (auto* cel = source->cel(0)) {
      doc::ImageRef image(render::rasterize_with_sprite_bounds(cel));
      if (source->isBackground()) image->setMaskColor(doc::color_t(-1));
      return image;
    }
  }
  doc::ImageRef image(doc::Image::create(sprite->pixelFormat(), sprite->width(), sprite->height()));
  image->setMaskColor(sprite->transparentColor());
  image->clear(sprite->transparentColor());
  if (sampleMode != 0) {
    render::Render renderer;
    renderer.setNewBlend(Preferences::instance().experimental.newBlend());
    renderer.setComposeGroups(true);
    renderer.setRefLayersVisiblity(false);
    if (sampleMode == 1)
      renderer.setLayerFilter([below](const doc::Layer* layer) { return below.count(layer) != 0; });
    renderer.renderSprite(image.get(), sprite, 0);
    auto* background = sprite->backgroundLayer();
    if (background && background->isVisibleHierarchy() &&
        (sampleMode == 2 || below.count(background))) image->setMaskColor(doc::color_t(-1));
  }
  return image;
}

void CloneStampInk::prepareInk(ToolLoop* loop)
{
  m_started = false;
}

void CloneStampInk::prepareForStrokes(ToolLoop* loop, Strokes& strokes)
{
  if (!m_sample || strokes.empty() || strokes[0].empty()) return;
  auto& state = loop->getDocument()->cloneStamp();
  if (!m_started) {
    state.beginStroke(strokes[0].firstPoint().toPoint());
    m_started = true;
  }
  state.currentDestination = strokes[0].lastPoint().toPoint();
  state.currentSource = state.currentDestination + state.offset;
}

void CloneStampInk::inkHline(int x1, int y, int x2, ToolLoop* loop)
{
  auto& state = loop->getDocument()->cloneStamp();
  if (!m_sample || !state.offsetValid || loop->getOpacity() == 0)
    return;
  auto* dst = loop->getDstImage();
  auto* original = loop->getSrcImage();
  const auto origin = loop->getCelOrigin();
  for (int x = x1; x <= x2; ++x) {
    // Scanlines are in destination-canvas coordinates; sampling is sprite-wide.
    const gfx::Point canvas(x + origin.x, y + origin.y);
    const gfx::Point sample = canvas + state.offset;
    if (!m_sample->bounds().contains(sample) || !dst->bounds().contains(gfx::Point(x, y)))
      continue;
    if (loop->useMask() && !loop->getMask()->containsPoint(canvas.x, canvas.y))
      continue;
    const doc::color_t src = m_sample->getPixel(sample.x, sample.y);
    const doc::color_t base = original->getPixel(x, y);
    doc::color_t result;
    switch (dst->pixelFormat()) {
      case doc::IMAGE_RGB:
        if (!doc::rgba_geta(src)) continue;
        result = doc::rgba_blender_normal(base, src, loop->getOpacity());
        break;
      case doc::IMAGE_GRAYSCALE:
        if (!doc::graya_geta(src)) continue;
        result = doc::graya_blender_normal(base, src, loop->getOpacity());
        break;
      case doc::IMAGE_INDEXED:
        if (src == loop->sprite()->transparentColor() && m_sample->maskColor() != doc::color_t(-1)) continue;
        if (loop->getOpacity() == 255 &&
            !(src == loop->sprite()->transparentColor() && !loop->getLayer()->isBackground() &&
              m_sample->maskColor() == doc::color_t(-1))) result = src;
        // An opaque background's reserved index must map to an opaque palette entry
        // when cloning into a transparent layer; otherwise the painted pixel disappears.
        else {
          auto* palette = loop->getPalette();
          doc::color_t backdrop = palette->getEntry(base);
          if (loop->getLayer()->isBackground())
            backdrop = doc::rgba(doc::rgba_getr(backdrop), doc::rgba_getg(backdrop), doc::rgba_getb(backdrop), 255);
          else if (base == loop->sprite()->transparentColor()) backdrop = 0;
          result = loop->getRgbMap()->mapColor(doc::rgba_blender_normal(
            backdrop,
            m_sample->maskColor() == doc::color_t(-1) ?
              doc::rgba(doc::rgba_getr(palette->getEntry(src)), doc::rgba_getg(palette->getEntry(src)),
                        doc::rgba_getb(palette->getEntry(src)), 255) : palette->getEntry(src),
            loop->getOpacity()));
        }
        break;
      default: continue;
    }
    dst->putPixel(x, y, result);
  }
}

}} // namespace app::tools
