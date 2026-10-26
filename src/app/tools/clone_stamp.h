// Aseprite Plus - hard-edged pixel clone stamp.
// Distributed under the End-User License Agreement for Aseprite.
#pragma once

#include "base/debug.h"
#include "app/tools/ink.h"
#include "doc/image_ref.h"
#include "doc/object.h"
#include "gfx/point.h"

#include <optional>

namespace app {
class Doc;

// Transient, document-owned state; never serialized or added to undo history.
struct CloneStampState {
  bool pending = false;
  bool valid = false;
  bool offsetValid = false;
  bool showSamplePair = false;
  doc::ObjectId sourceLayer = doc::NullId;
  gfx::Point anchor;
  gfx::Point offset;
  gfx::Point currentSource;
  gfx::Point currentDestination;
  std::optional<gfx::Point> pendingMarker;

  void requestSample(std::optional<gfx::Point> marker = std::nullopt) {
    pending = true;
    pendingMarker = marker;
  }
  void cancelSample() {
    pending = false;
    pendingMarker.reset();
  }
  gfx::Point previewSource(const gfx::Point& destination) const {
    return offsetValid ? destination + offset : anchor;
  }

  void sample(const gfx::Point& point, doc::ObjectId layer) {
    anchor = currentSource = point;
    sourceLayer = layer;
    valid = true;
    // Y records the destination before the source click, so the pair defines
    // the offset immediately instead of waiting for the first painted pixel.
    offsetValid = showSamplePair = pendingMarker.has_value();
    currentDestination = pendingMarker.value_or(point);
    if (offsetValid) offset = anchor - currentDestination;
    cancelSample();
  }
  void beginStroke(const gfx::Point& destination) {
    if (!offsetValid) {
      offset = anchor - destination;
      offsetValid = true;
    }
    showSamplePair = false;
    currentDestination = destination;
    currentSource = destination + offset;
  }
};

namespace tools {
// 0: sampled layer, 1: sampled layer and visible layers below, 2: all visible.
doc::ImageRef cloneStampSnapshot(Doc* document, int sampleMode);

class CloneStampInk : public Ink {
public:
  Ink* clone() override { return new CloneStampInk; }
  bool isPaint() const override { return true; }
  bool isEffect() const override { return true; } // Own ink; keep opacity controls.
  void setSnapshot(const doc::ImageRef& image) { m_sample = image; }
  void prepareInk(ToolLoop* loop) override;
  void prepareForStrokes(ToolLoop* loop, Strokes& strokes) override;
  void inkHline(int x1, int y, int x2, ToolLoop* loop) override;
private:
  bool m_started = false;
  doc::ImageRef m_sample;
};
} // namespace tools
} // namespace app
