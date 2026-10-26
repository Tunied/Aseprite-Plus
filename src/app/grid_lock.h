// Transient editing constraint, independent of the document selection and undo history.
#ifndef APP_GRID_LOCK_H_INCLUDED
#define APP_GRID_LOCK_H_INCLUDED
#pragma once

#include "doc/grid.h"
#include "gfx/rect.h"

namespace app {

struct GridLockState {
  bool pending = false;
  gfx::Rect bounds;

  bool locked() const { return !bounds.isEmpty(); }
  bool enabled() const { return pending || locked(); }
  void reset() { pending = false; bounds = gfx::Rect(); }
  void request() { reset(); pending = true; }

  bool select(const gfx::Point& point, const gfx::Rect& gridBounds,
              const gfx::Rect& spriteBounds)
  {
    if (!pending || gridBounds.w < 1 || gridBounds.h < 1 || !spriteBounds.contains(point))
      return false;
    const doc::Grid grid(gridBounds);
    bounds = grid.tileBoundsInCanvas(grid.canvasToTile(point)) & spriteBounds;
    pending = false;
    return locked();
  }
};

} // namespace app
#endif
