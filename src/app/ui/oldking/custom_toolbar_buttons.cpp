// Aseprite OldKing
// Distributed under the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/oldking/custom_toolbar_buttons.h"

#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/commands/params.h"
#include "app/context.h"
#include "app/i18n/strings.h"
#include "app/ui/keyboard_shortcuts.h"
#include "app/ui/skin/skin_theme.h"
#include "os/surface.h"
#include "ui/graphics.h"
#include "ui/system.h"

#include <algorithm>

namespace app { namespace oldking {

CustomToolbarButtons::CustomToolbarButtons()
{
  registerCustomToolbarEntries(*this);
}

void CustomToolbarButtons::registerButton(const CustomToolbarButtonEntry& entry)
{
  auto it = std::find_if(m_entries.begin(), m_entries.end(),
                         [&](const auto& other) { return other.id == entry.id; });
  if (it == m_entries.end())
    m_entries.push_back(entry);
  else
    *it = entry;
}

const CustomToolbarButtonEntry* CustomToolbarButtons::entry(int button) const
{
  return button >= 0 && button < count() ? &m_entries[button] : nullptr;
}

bool CustomToolbarButtons::isCustomIndex(int index) const
{
  return index <= BaseIndex && index > BaseIndex - count();
}

int CustomToolbarButtons::reservedHeight(const gfx::Size& iconSize, int scale) const
{
  // Native vertical step, final button border, and two small section gaps.
  return count() ? count() * (iconSize.h - scale) + 7 * scale : 0;
}

gfx::Rect CustomToolbarButtons::buttonBounds(int button,
                                            const gfx::Rect& previewBounds,
                                            const gfx::Size& iconSize,
                                            int scale) const
{
  if (!entry(button))
    return gfx::Rect();
  const int step = iconSize.h - scale;
  return gfx::Rect(previewBounds.x,
                   previewBounds.y - reservedHeight(iconSize, scale) + 2 * scale + button * step,
                   previewBounds.w,
                   iconSize.h + (button == count() - 1 ? 2 : 1) * scale);
}

int CustomToolbarButtons::hitTest(const gfx::Point& pos,
                                 const gfx::Rect& previewBounds,
                                 const gfx::Size& iconSize,
                                 int scale) const
{
  // Native skins overlap their border pixels. The later painted button owns them.
  for (int button = count() - 1; button >= 0; --button) {
    if (buttonBounds(button, previewBounds, iconSize, scale).contains(pos))
      return button;
  }
  return -1;
}

Command* CustomToolbarButtons::command(int button) const
{
  const auto* e = entry(button);
  return e ? Commands::instance()->byId(e->commandId.c_str()) : nullptr;
}

bool CustomToolbarButtons::isEnabled(int button, Context* context) const
{
  auto* cmd = command(button);
  if (!cmd || !context)
    return false;
  context->updateFlags();
  cmd->loadParams(Params());
  return cmd->isEnabled(context);
}

bool CustomToolbarButtons::isChecked(int button, Context* context) const
{
  auto* cmd = command(button);
  if (!cmd || !context)
    return false;
  context->updateFlags();
  cmd->loadParams(Params());
  return cmd->isChecked(context);
}

void CustomToolbarButtons::execute(int button, Context* context) const
{
  if (!isEnabled(button, context))
    return;
  // Use the menu/shortcut guard as well as the official command dispatch.
  if (auto* cmd = command(button))
    context->executeCommandFromMenuOrShortcut(cmd, Params());
}

std::string CustomToolbarButtons::tooltip(int button) const
{
  const auto* e = entry(button);
  if (!e)
    return std::string();
  auto* cmd = command(button);
  std::string text = cmd ? cmd->friendlyName() : std::string();
  if (text.empty() || text == e->commandId)
    text = e->tooltip;

  if (cmd) {
    const auto key = KeyboardShortcuts::instance()->command(e->commandId.c_str());
    if (key && !key->shortcuts().empty())
      text += "\n\n" + Strings::tools_shortcut(key->shortcuts().front().toString());
  }
  return text;
}

void CustomToolbarButtons::paint(ui::Graphics* g,
                                const ui::Widget* toolbar,
                                int button,
                                const gfx::Rect& bounds,
                                bool hot,
                                Context* context) const
{
  const auto* e = entry(button);
  if (!e)
    return;
  auto* theme = skin::SkinTheme::get(toolbar);
  const bool enabled = isEnabled(button, context);
  const bool checked = isChecked(button, context);
  // Preview/Timeline use toolbuttonHot for checked; match those toggles.
  auto part = enabled && (hot || checked) ? theme->parts.toolbuttonHot() :
              button == count() - 1 ? theme->parts.toolbuttonLast() :
                                     theme->parts.toolbuttonNormal();
  theme->drawRect(g, bounds, part.get());
  if (auto* icon = theme->getToolIcon(e->iconId.c_str())) {
    const int x = ui::guiscaled_center(bounds.x, bounds.w, icon->width());
    const int y = ui::guiscaled_center(bounds.y, bounds.h, icon->height());
    if (enabled)
      g->drawRgbaSurface(icon, x, y);
    else
      g->drawColoredRgbaSurface(icon, theme->colors.disabled(), x, y);
  }
}

} } // namespace app::oldking
