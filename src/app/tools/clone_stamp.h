// Aseprite Plus - hard-edged pixel clone stamp.
// Distributed under the End-User License Agreement for Aseprite.
#pragma once

#include "base/debug.h"
#include "app/tools/ink.h"
#include "doc/image_ref.h"
#include "doc/object.h"
#include "gfx/point.h"

namespace app {
class Doc;

// Transient, document-owned state; never serialized or added to undo history.
struct CloneStampState {
  bool pending = false;
  bool valid = false;
  bool offsetValid = false;
  doc::ObjectId sourceLayer = doc::NullId;
  gfx::Point anchor;
  gfx::Point offset;
  gfx::Point currentSource;

  void sample(const gfx::Point& point, doc::ObjectId layer) {
    anchor = currentSource = point;
    sourceLayer = layer;
    valid = true;
    pending = offsetValid = false;
  }
  void beginStroke(const gfx::Point& destination, bool aligned) {
    if (!aligned || !offsetValid) {
      offset = anchor - destination;
      offsetValid = true;
    }
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
