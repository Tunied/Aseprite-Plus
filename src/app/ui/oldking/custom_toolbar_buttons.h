// Aseprite OldKing
// Distributed under the End-User License Agreement for Aseprite.

#ifndef APP_UI_OLDKING_CUSTOM_TOOLBAR_BUTTONS_H_INCLUDED
#define APP_UI_OLDKING_CUSTOM_TOOLBAR_BUTTONS_H_INCLUDED
#pragma once

#include "gfx/point.h"
#include "gfx/rect.h"
#include "gfx/size.h"

#include <string>
#include <vector>

namespace ui {
class Graphics;
class Widget;
}

namespace app {
class Command;
class Context;

namespace oldking {

class ToolBarTestAccess;

struct CustomToolbarButtonEntry {
  std::string id;
  std::string commandId;
  std::string iconId;
  std::string tooltip;
};

// Data and stateless helpers only. ToolBar owns mouse/capture/tips/popups.
class CustomToolbarButtons {
public:
  static constexpr int BaseIndex = -1000;

  CustomToolbarButtons();
  void registerButton(const CustomToolbarButtonEntry& entry);
  int count() const { return int(m_entries.size()); }
  const CustomToolbarButtonEntry* entry(int button) const;

  static int toolbarIndex(int button) { return BaseIndex - button; }
  bool isCustomIndex(int index) const;
  static int buttonIndex(int index) { return BaseIndex - index; }

  int reservedHeight(const gfx::Size& iconSize, int scale) const;
  gfx::Rect buttonBounds(int button,
                         const gfx::Rect& previewBounds,
                         const gfx::Size& iconSize,
                         int scale) const;
  int hitTest(const gfx::Point& pos,
              const gfx::Rect& previewBounds,
              const gfx::Size& iconSize,
              int scale) const;

  // No Command pointer survives a query or dispatch (extensions can reload).
  Command* command(int button) const;
  bool isEnabled(int button, Context* context) const;
  bool isChecked(int button, Context* context) const;
  void execute(int button, Context* context) const;
  std::string tooltip(int button) const;
  void paint(ui::Graphics* g,
             const ui::Widget* toolbar,
             int button,
             const gfx::Rect& bounds,
             bool hot,
             Context* context) const;

private:
  std::vector<CustomToolbarButtonEntry> m_entries;
};

void registerCustomToolbarEntries(CustomToolbarButtons& buttons);

} // namespace oldking
} // namespace app

#endif
