// Exercise document preference connections without opening native windows.
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "app/app.h"
#include "app/cli/app_options.h"
#include "app/commands/commands.h"
#include "app/doc.h"
#include "app/docs.h"
#include "app/fonts/font_info.h"
#include "app/pref/preferences.h"
#include "app/tools/tool_box.h"
#include "app/ui/button_set.h"
#include "app/ui/color_bar.h"
#include "app/ui/context_bar.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui_context.h"
#include "os/system.h"
#include "ui/manager.h"
#include "ui/system.h"
#include "ui/theme.h"
#include "ui/tooltips.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool value, const char* message)
{
  ++checks;
  if (!value) throw std::runtime_error(message);
}
void connections(app::Doc* doc, bool connected)
{
  auto& pref = app::Preferences::instance().document(doc);
  check(bool(pref.show.grid.AfterChange) == connected, "grid visibility connection lifetime");
  check(bool(pref.grid.bounds.AfterChange) == connected, "grid bounds connection lifetime");
}
}

int app_main(int, char** argv)
{
  try {
    check(std::getenv("ASEPRITE_USER_FOLDER") != nullptr, "isolated preferences required");
    auto system = os::System::make();
    const char* args[] = {argv[0], "--batch"};
    app::AppOptions options(2, args);
    app::App app;
    check(app.initialize(options) == 0, "batch initialization");
    ui::UISystem uiSystem;
    app::skin::SkinTheme theme;
    ui::set_theme(&theme, 1);
    // Normal GUI initialization supplies a main-window font fallback. This
    // windowless fixture supplies the same font through isolated preferences.
    auto fontInfo = theme.getDefaultFontInfo();
    fontInfo.updatePreferences();
    ui::Manager manager(nullptr);
    // Drain queued widget messages at teardown without flipping a native
    // display: this fixture deliberately has no OS window to present to.
    manager._closingAppWithException();
    ui::TooltipManager tooltips;
    app::ColorBar colors(&tooltips);
    {
      app::ContextBar bar(&tooltips, &colors);
      manager.addChild(&bar);
      auto* grid = bar.findChildT<app::ButtonSet>("grid_view");
      auto* lock = bar.findChildT<app::ButtonSet>("grid_lock");
      check(grid && lock && grid != lock, "grid view has a separate button from grid focus");
      check(grid->isVisible() && !grid->isEnabled(), "grid button starts visible but disabled");
      auto* context = app::UIContext::instance();
      auto& docs = context->documents();
      auto* first = docs.add(16, 16, doc::ColorMode::RGB, 256);
      auto* second = docs.add(16, 16, doc::ColorMode::RGB, 256);
      context->setActiveDocument(first);
      connections(first, true);
      auto& firstPref = app::Preferences::instance().document(first);
      firstPref.show.grid(false);
      grid->ItemChange(grid->getItem(0));
      check(firstPref.show.grid() && grid->getItem(0)->isSelected(), "grid button shows grid");
      check(!first->gridLock().enabled(), "showing grid does not request a focused cell");
      grid->ItemChange(grid->getItem(0));
      check(!firstPref.show.grid() && !grid->getItem(0)->isSelected(), "grid button hides grid");
      context->executeCommand(app::Commands::instance()->byId(app::CommandId::ShowGrid()));
      check(grid->getItem(0)->isSelected(), "menu command updates grid button");
      for (const char* id : {"pencil", "rectangular_marquee", "clone_stamp", "paint_bucket", "eyedropper", "hand"}) {
        bar.updateForTool(app.toolBox()->getToolById(id));
        bar.setBounds(gfx::Rect(0, 0, 1000, bar.sizeHint().h));
        check(grid->isVisible() && grid->bounds().w > 0 && grid->getItem(0)->iconSurface(),
              "grid view stays visible across tool changes");
        check(grid->bounds().x2() <= lock->bounds().x, "grid view precedes focus button");
      }
      app::Preferences::instance().document(second).show.grid(false);
      context->setActiveDocument(second);
      check(!grid->getItem(0)->isSelected(), "document switch updates grid button");
      connections(first, false);
      connections(second, true);
      // Returning to an empty site must detach before closing its document.
      context->setActiveDocument(nullptr);
      check(!grid->isEnabled() && !grid->getItem(0)->isSelected(), "empty site disables grid button");
      connections(second, false);
      second->close();
      delete second;
      context->setActiveDocument(first);
      check(grid->isEnabled() && grid->getItem(0)->isSelected(), "return restores document grid state");
      connections(first, true);
      // Normal exit closes documents before destroying ContextBar.
      app.run(false);
      check(docs.empty(), "exit closes all documents before toolbar destruction");
    }
    std::cout << "PASS: " << checks << " context bar lifecycle checks (no windows)\n";
    return 0;
  }
  catch (const std::exception& e) {
    std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
    return 1;
  }
}
