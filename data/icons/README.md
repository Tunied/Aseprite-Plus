# Grid lock icon

`grid-lock.svg` is the transparent **Padlock** icon by **Lorc**, copied from
`GameIcons/透明_SVG/1x1/lorc/padlock.svg`.

- Source: https://game-icons.net/1x1/lorc/padlock.html
- Author: https://lorcblog.blogspot.com/
- License: Creative Commons Attribution 3.0 Unported (see `GameIcons-LICENSE.txt`).
- Adaptation: scaled to a 14×14 alpha mask with a 1px margin; the editor tints it
  using the active theme text color and scales it with the other toolbar icons.

The checked-in `src/app/ui/oldking/grid_lock_icon.h` keeps this icon independent
of theme sprite sheets and avoids a runtime SVG loader. To regenerate it, run
`misc/generate_grid_lock_icon.cjs` with Node.js and `sharp` available.
