// Fractional screen-pixel accumulation for editor-edge auto-scrolling.
#ifndef APP_UI_EDITOR_EDGE_SCROLL_H_INCLUDED
#define APP_UI_EDITOR_EDGE_SCROLL_H_INCLUDED
#pragma once

#include "gfx/point.h"

#include <algorithm>
#include <cstdint>

namespace app {

class EdgeScroll {
public:
  void reset() { m_remainder = gfx::Point(0, 0); }

  gfx::Point scale(const gfx::Point& delta, int percent)
  {
    percent = std::clamp(percent, 1, 100);
    return gfx::Point(scaleAxis(delta.x, percent, m_remainder.x),
                      scaleAxis(delta.y, percent, m_remainder.y));
  }

private:
  static int scaleAxis(int delta, int percent, int& remainder)
  {
    // A zero delta must retain the fraction: devices can report axes separately.
    // Direction reversals must respond without carrying the old direction's fraction.
    if ((delta < 0 && remainder > 0) || (delta > 0 && remainder < 0))
      remainder = 0;
    const int64_t total = int64_t(delta) * percent + remainder;
    remainder = int(total % 100);
    return int(total / 100);
  }

  gfx::Point m_remainder = gfx::Point(0, 0);
};

} // namespace app

#endif
