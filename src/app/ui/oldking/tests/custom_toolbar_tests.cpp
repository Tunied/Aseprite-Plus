// Aseprite OldKing focused regression runner.
// Distributed under the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/app.h"
#include "app/cli/app_options.h"
#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/context.h"
#include "app/pref/preferences.h"
#include "app/script/engine.h"
#include "app/tools/tool_box.h"
#include "app/tools/tool.h"
#include "app/ui/app_menuitem.h"
#include "app/ui/dock.h"
#include "app/ui/keyboard_shortcuts.h"
#include "app/ui/main_window.h"
#include "app/ui/oldking/custom_toolbar_buttons.h"
#include "app/ui/preview_editor.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/toolbar.h"
#include "os/system.h"
#include "ui/graphics.h"
#include "ui/manager.h"
#include "ui/message.h"
#include "ui/paint_event.h"
#include "ui/popup_window.h"
#include "ui/tooltips.h"

#include <iostream>
#include <stdexcept>

namespace app { namespace oldking {

class ToolBarTestAccess {
public:
  static gfx::Rect bounds(ToolBar* bar, int index) { return bar->getToolGroupBounds(index); }
  static int hidden(ToolBar* bar) { return bar->getHiddenGroups(); }
  static int hot(ToolBar* bar) { return bar->m_hotIndex; }
  static ui::TipWindow* tip(ToolBar* bar) { return bar->m_tipWindow.get(); }
  static bool left(ToolBar* bar) { return bar->isDockedAtLeftSide(); }
  static bool popup(ToolBar* bar)
  {
    return bar->m_popupWindow && bar->m_popupWindow->isVisible();
  }
  static CustomToolbarButtons& buttons(ToolBar* bar) { return bar->m_customButtons; }
  static void closePopup(ToolBar* bar) { bar->closePopupWindow(); }
  static void showTip(ToolBar* bar)
  {
    ui::TimerMessage tick(1, &bar->m_tipTimer);
    bar->onProcessMessage(&tick);
  }
  static void paint(ToolBar* bar, ui::Graphics* g)
  {
    ui::PaintEvent ev(bar, g);
    bar->onPaint(ev);
  }
};

} }

namespace {
int checks = 0;
void check(bool condition, const char* message)
{
  ++checks;
  if (!condition) {
    std::cerr << "FAILED CHECK " << checks << ": " << message << std::endl;
    throw std::runtime_error(message);
  }
}

class StateCommand : public app::Command {
public:
  StateCommand() : Command("OldKingToolbarStateTest") {}
  bool enabled = true;
  bool checked = true;
  int calls = 0;
protected:
  bool onEnabled(app::Context*) override { return enabled; }
  bool onChecked(app::Context*) override { return checked; }
  void onExecute(app::Context*) override { ++calls; }
};

class MenuInvoker : public app::AppMenuItem {
public:
  MenuInvoker(const std::string& command) : AppMenuItem("Toolbar regression", command) {}
  void click() { onClick(); }
};

void mouse(app::ToolBar* bar, ui::MessageType type, const gfx::Point& point)
{
  ui::MouseMessage msg(type, ui::PointerType::Mouse, ui::kButtonLeft,
                       ui::kKeyNoneModifier, point);
  msg.setDisplay(bar->display());
  bar->sendMessage(&msg);
}

void testLayout(const app::oldking::CustomToolbarButtons& buttons)
{
  for (int scale : { 1, 2, 3, 4 }) {
    const gfx::Size size(16 * scale, 16 * scale);
    for (int x : { 10 * scale, 500 * scale }) {
      const gfx::Rect preview(x, 240 * scale, size.w, size.h + 2 * scale);
      check(buttons.hitTest(preview.center(), preview, size, scale) == -1,
            "preview hit by a custom button");
      for (int b = 0; b < buttons.count(); ++b) {
        const auto rc = buttons.buttonBounds(b, preview, size, scale);
        check(rc.y2() <= preview.y - 2 * scale, "custom button overlaps preview");
        check(rc.w == preview.w, "custom width differs from native");
        check(buttons.hitTest(rc.center(), preview, size, scale) == b, "wrong hit test");
        check(buttons.isCustomIndex(buttons.toolbarIndex(b)), "invalid custom encoding");
        check(buttons.buttonIndex(buttons.toolbarIndex(b)) == b, "index round trip");
        if (b > 0) {
          check(rc.y - buttons.buttonBounds(b - 1, preview, size, scale).y == size.h - scale,
                "vertical step differs from native");
          check(buttons.hitTest(gfx::Point(rc.x + 1, rc.y), preview, size, scale) == b,
                "shared skin border dispatches to two buttons");
        }
      }
      check(buttons.hitTest(gfx::Point(preview.x - 1, preview.y - 20 * scale),
                            preview, size, scale) == -1, "out of bounds horizontal hit");
    }
    // Adding a fifth entry must affect layout without changing ToolBar logic.
    auto extended = buttons;
    extended.registerButton({ "fifth-test", "missing", "pencil", "Test" });
    check(extended.count() == buttons.count() + 1, "registration failed");
    check(extended.reservedHeight(size, scale) - buttons.reservedHeight(size, scale) ==
            size.h - scale, "fifth button reservation failed");
  }
  check(!buttons.isCustomIndex(-1) && !buttons.isCustomIndex(-2) &&
          !buttons.isCustomIndex(-3) && !buttons.isCustomIndex(0), "native index collision");
}

void testCommands(app::App& app, app::oldking::CustomToolbarButtons& buttons)
{
  auto* ctx = app.context();
  auto extra = buttons;
  const int test = extra.count();
  extra.registerButton({ "missing-test", "OldKingToolbarMissingTest", "pencil", "Missing" });
  check(extra.command(test) == nullptr, "missing command resolved");
  check(!extra.isEnabled(test, ctx) && !extra.isChecked(test, ctx), "missing command state");
  check(extra.tooltip(test) == "Missing", "missing tooltip fallback");
  extra.execute(test, ctx);
  check(!extra.isEnabled(-1, ctx), "invalid index enabled");

  const int state = extra.count();
  extra.registerButton({ "state-test", "OldKingToolbarStateTest", "pencil", "State" });
  {
    StateCommand cmd;
    app::Commands::instance()->add(&cmd);
    check(extra.isEnabled(state, ctx) && extra.isChecked(state, ctx), "checked command state");
    extra.execute(state, ctx);
    check(cmd.calls == 1, "command not dispatched");
    cmd.enabled = false;
    cmd.checked = false;
    check(!extra.isEnabled(state, ctx) && !extra.isChecked(state, ctx), "disabled state stale");
    extra.execute(state, ctx);
    check(cmd.calls == 1, "disabled command executed");
    app::Commands::instance()->remove(&cmd);
  }
  check(extra.command(state) == nullptr, "cached deleted command");
  {
    StateCommand replacement;
    app::Commands::instance()->add(&replacement);
    extra.execute(state, ctx);
    check(replacement.calls == 1, "replacement command not resolved lazily");
    app::Commands::instance()->remove(&replacement);
  }

  // Observe each real installed plugin's isolated Lua entry point.
  // Batch mode records dispatch only; GUI mode also opens its actual Dialog.
  check(app.scriptEngine()->evalCode(R"(
    _toolbar_test_calls = {}
    _toolbar_test_opened = {}
    _toolbar_test_originals = {}
    local root = assert(os.getenv('ASEPRITE_USER_FOLDER'), 'Use an isolated test user folder')
    for _, plugin in ipairs({ 'ce-click-flood-fill-rect', 'ce-gen-shape',
                              'oldking-dual-grid-map-export', 'ce-extraction-mask-painter' }) do
      package.path = root .. '/extensions/' .. plugin .. '/?.lua;' ..
                     root .. '/extensions/' .. plugin .. '/src/?.lua;' .. package.path
    end
    local names = { 'ce_flood_main', 'ce_shape_main', 'oldking_dual_grid_main', 'ce_emp_main' }
    for _, name in ipairs(names) do
      local main = require(name)
      local original = main.open_singleton
      _toolbar_test_originals[name] = original
      main.open_singleton = function()
        _toolbar_test_calls[name] = (_toolbar_test_calls[name] or 0) + 1
        if app.isUIAvailable then
          _toolbar_test_opened[name] = false
          local instance = original()
          _toolbar_test_opened[name] = instance and instance.dialog ~= nil
          assert(instance and instance.dialog, 'Plugin Dialog was not created: ' .. name)
          return instance
        end
      end
    end
    if app.isUIAvailable then Sprite(64, 64) end
  )"), "failed to install runtime observers");

  for (int b = 0; b < buttons.count(); ++b) {
    auto* cmd = buttons.command(b);
    check(cmd && cmd->isPlugin(), "installed plugin command not registered");
    check(buttons.isEnabled(b, ctx) && !buttons.isChecked(b, ctx), "plugin command state");
    check(buttons.tooltip(b).find(cmd->friendlyName()) == 0, "plugin title missing from tooltip");
    check(app::KeyboardShortcuts::instance()->command(cmd->id().c_str()) != nullptr,
          "plugin command absent from KeyboardShortcuts");
    buttons.execute(b, ctx);
    check(app.scriptEngine()->evalCode(R"(
      for name, opened in pairs(_toolbar_test_opened) do
        assert(opened, 'Plugin did not open its Dialog: ' .. name)
      end
    )"), "real plugin Dialog failed");
  }
  check(app.scriptEngine()->evalCode(R"(
    local total = 0
    for name, calls in pairs(_toolbar_test_calls) do
      assert(calls == 1, 'Wrong plugin dispatch: ' .. name)
      total = total + calls
      require(name).shutdown_singleton()
    end
    _toolbar_test_total = total
  )"), "plugin dispatch or Dialog close failed");
  check(app.scriptEngine()->evalCode("assert(_toolbar_test_total == " +
                                    std::to_string(buttons.count()) + ")"),
        "plugin callbacks did not execute");

  const auto* e = buttons.entry(0);
  auto key = app::KeyboardShortcuts::instance()->command(e->commandId.c_str());
  key->add(ui::Shortcut(ui::kKeyCtrlModifier, ui::kKeyF12, 0), app::KeySource::UserDefined,
            *app::KeyboardShortcuts::instance());
  check(buttons.tooltip(0).find(key->shortcuts().front().toString()) != std::string::npos,
        "configured shortcut missing from tooltip");
  ui::KeyMessage keyMsg(ui::kKeyDownMessage, ui::kKeyF12, ui::kKeyCtrlModifier, 0, 0);
  app::Command* shortcutCommand = nullptr;
  app::Params params;
  check(app::KeyboardShortcuts::instance()->getCommandFromKeyMessage(
          &keyMsg, &shortcutCommand, &params, app::KeyContext::Any) &&
          shortcutCommand == buttons.command(0), "shortcut resolves to a different command");
  check(app.scriptEngine()->evalCode("_toolbar_shortcut_before = _toolbar_test_calls.ce_flood_main"),
        "failed to prepare shortcut dispatch check");
  ctx->executeCommandFromMenuOrShortcut(shortcutCommand, params);
  check(app.scriptEngine()->evalCode(R"(
    assert(_toolbar_test_calls.ce_flood_main == _toolbar_shortcut_before + 1)
    if app.isUIAvailable then assert(_toolbar_test_opened.ce_flood_main) end
    require('ce_flood_main').shutdown_singleton()
  )"), "shortcut did not execute the same plugin");
}

void testGui(app::App& app, app::oldking::CustomToolbarButtons& buttons)
{
  using app::oldking::ToolBarTestAccess;
  auto* bar = app.mainWindow()->toolBar();
  auto* dock = app.mainWindow()->customizableDock();
  for (int scale : { 1, 2 }) {
    ui::set_theme(ui::get_theme(), scale);
    for (int side : { ui::LEFT, ui::RIGHT }) {
      dock->undock(bar);
      dock->center()->dock(side, bar);
      dock->layout();
      check(bar->window() != nullptr, "test failed to attach the toolbar to a window");
      check(ToolBarTestAccess::left(bar) == (side == ui::LEFT), "dock side detection failed");
      const auto originalBounds = bar->bounds();
      for (int height = bar->sizeHint().h; height <= 500 * scale; ++height) {
        bar->setBounds(gfx::Rect(originalBounds.x, originalBounds.y, originalBounds.w, height));
        const int hidden = ToolBarTestAccess::hidden(bar);
        const int visible = app.toolBox()->getGroupsCount() - hidden;
        const auto topCustom = ToolBarTestAccess::bounds(bar, buttons.toolbarIndex(0));
        const auto last = ToolBarTestAccess::bounds(bar, hidden ? visible : visible - 1);
        check(last.y2() <= topCustom.y, "drawing/overflow button overlaps fixed actions");
        check(topCustom.y >= bar->bounds().y, "fixed action clipped above toolbar");
        check(ToolBarTestAccess::bounds(bar, app::ToolBar::TimelineVisibilityIndex).y2() <=
                bar->bounds().y2(), "timeline clipped");
      }
      bar->setBounds(originalBounds);
      auto* active = app.activeTool();
      for (int b = 0; b < buttons.count(); ++b) {
        const auto rc = ToolBarTestAccess::bounds(bar, buttons.toolbarIndex(b));
        mouse(bar, ui::kMouseLeaveMessage, rc.center());
        mouse(bar, ui::kMouseMoveMessage, rc.center());
        check(ToolBarTestAccess::hot(bar) == buttons.toolbarIndex(b), "custom hover failed");
        check(ToolBarTestAccess::tip(bar) != nullptr, "native tooltip not created");
        ToolBarTestAccess::showTip(bar);
        check(ToolBarTestAccess::tip(bar)->isVisible(), "tooltip timer did not open native window");
        mouse(bar, ui::kMouseDownMessage, rc.center());
        check(!bar->hasCapture(), "custom action captured the mouse");
        check(app.activeTool() == active, "custom action changed drawing tool");
        check(app.scriptEngine()->evalCode(R"(
          for name, opened in pairs(_toolbar_test_opened) do assert(opened, name) end
        )"), "toolbar click failed to open real plugin Dialog");
        check(app.scriptEngine()->evalCode(R"(
          for name, calls in pairs(_toolbar_test_calls) do require(name).shutdown_singleton() end
        )"), "failed to close plugin panels");
        const char* modules[] = { "ce_flood_main", "ce_shape_main", "oldking_dual_grid_main", "ce_emp_main" };
        const std::string module = modules[b];
        check(app.scriptEngine()->evalCode(
                "_toolbar_menu_before = _toolbar_test_calls['" + module + "']"),
              "failed to prepare menu comparison");
        MenuInvoker menu(buttons.entry(b)->commandId);
        menu.click();
        check(app.scriptEngine()->evalCode(
                "assert(_toolbar_test_calls['" + module + "'] == _toolbar_menu_before + 1); "
                "assert(_toolbar_test_opened['" + module + "']); "
                "require('" + module + "').shutdown_singleton()"),
              "menu and toolbar executed different plugin functions");
      }
      bar->closeTipWindow();

      // A missing command must also survive a real Toolbar click, without Lua dispatch.
      const auto savedEntry = *buttons.entry(0);
      ToolBarTestAccess::buttons(bar).registerButton(
        { savedEntry.id, "OldKingToolbarMissingTest", savedEntry.iconId, savedEntry.tooltip });
      mouse(bar, ui::kMouseDownMessage, ToolBarTestAccess::bounds(bar, buttons.toolbarIndex(0)).center());
      check(app.activeTool() == active && !bar->hasCapture(), "missing command altered tool/capture");
      ToolBarTestAccess::buttons(bar).registerButton(savedEntry);

      // Compare actual skin rendering of normal/hot/checked/disabled, without screenshots.
      auto stateButtons = buttons;
      stateButtons.registerButton(
        { savedEntry.id, "OldKingToolbarStateTest", savedEntry.iconId, savedEntry.tooltip });
      StateCommand stateCommand;
      app::Commands::instance()->add(&stateCommand);
      auto stateSurface = os::System::instance()->makeRgbaSurface(20 * scale, 20 * scale);
      ui::Graphics stateGraphics(stateSurface);
      const auto stateBounds = stateSurface->bounds();
      auto pixels = [&](bool hot) {
        stateGraphics.fillRect(app::skin::SkinTheme::get(bar)->colors.tabActiveFace(), stateBounds);
        stateButtons.paint(&stateGraphics, bar, 0, stateBounds, hot, app.context());
        std::vector<gfx::Color> result;
        for (int y = 0; y < stateBounds.h; ++y)
          for (int x = 0; x < stateBounds.w; ++x)
            result.push_back(stateGraphics.getPixel(x, y));
        return result;
      };
      stateCommand.checked = false;
      const auto normal = pixels(false);
      const auto hot = pixels(true);
      stateCommand.checked = true;
      const auto checked = pixels(false);
      check(normal != hot && hot == checked, "checked/hover skin rendering failed");
      stateCommand.enabled = false;
      check(pixels(true) != hot, "disabled icon rendering failed");
      app::Commands::instance()->remove(&stateCommand);

      auto surface = os::System::instance()->makeRgbaSurface(bar->size().w, bar->size().h);
      ui::Graphics g(surface);
      ToolBarTestAccess::paint(bar, &g);
      for (int b = 0; b < buttons.count(); ++b)
        check(app::skin::SkinTheme::get(bar)->getToolIcon(buttons.entry(b)->iconId.c_str()) != nullptr,
              "native icon missing at GUI scale");

      // Native Preview/Timeline are still dispatched by the same ToolBar.
      auto* preview = app.mainWindow()->getPreviewEditor();
      bool enabled = preview->isPreviewEnabled();
      mouse(bar, ui::kMouseDownMessage,
            ToolBarTestAccess::bounds(bar, app::ToolBar::PreviewVisibilityIndex).center());
      check(preview->isPreviewEnabled() != enabled, "native preview toggle failed");
      preview->setPreviewEnabled(enabled);
      enabled = app.mainWindow()->getTimelineVisibility();
      mouse(bar, ui::kMouseDownMessage,
            ToolBarTestAccess::bounds(bar, app::ToolBar::TimelineVisibilityIndex).center());
      check(app.mainWindow()->getTimelineVisibility() != enabled, "native timeline toggle failed");
      app.mainWindow()->setTimelineVisibility(enabled);

      // Drawing group selection/popups and the native overflow still use ToolStrip.
      bar->setBounds(gfx::Rect(originalBounds.x, originalBounds.y, originalBounds.w, 500 * scale));
      auto* firstGroup = *app.toolBox()->begin_group();
      int toolsInGroup = 0;
      for (auto* tool : *app.toolBox())
        if (tool->getGroup() == firstGroup)
          ++toolsInGroup;
      auto toolPos = ToolBarTestAccess::bounds(bar, 0).center();
      mouse(bar, ui::kMouseDownMessage, toolPos);
      check(app.activeTool()->getGroup() == firstGroup, "native tool selection failed");
      if (toolsInGroup > 1)
        check(ToolBarTestAccess::popup(bar), "native tool group popup failed");
      mouse(bar, ui::kMouseUpMessage, toolPos);
      mouse(bar, ui::kMouseDownMessage, toolPos);
      mouse(bar, ui::kMouseUpMessage, toolPos);
      check(!bar->hasCapture(), "native tool group did not release capture");
      // The native MouseUp can retain a closed PopupWindow until the next action.
      ToolBarTestAccess::closePopup(bar);

      bar->setBounds(gfx::Rect(originalBounds.x, originalBounds.y, originalBounds.w, bar->sizeHint().h));
      check(ToolBarTestAccess::hidden(bar) > 0, "short toolbar did not hide tool groups");
      const int visible = app.toolBox()->getGroupsCount() - ToolBarTestAccess::hidden(bar);
      mouse(bar, ui::kMouseDownMessage, ToolBarTestAccess::bounds(bar, visible).center());
      check(ToolBarTestAccess::popup(bar), "native overflow popup failed");
      mouse(bar, ui::kMouseDownMessage, ToolBarTestAccess::bounds(bar, buttons.toolbarIndex(0)).center());
      check(!ToolBarTestAccess::popup(bar), "custom action did not close native overflow");
      check(app.scriptEngine()->evalCode("require('ce_flood_main').shutdown_singleton()"),
            "failed to close flood dialog after overflow");
      bar->selectTool(active);
      bar->setBounds(originalBounds);
    }
  }
}
} // namespace

int app_main(int argc, char* argv[])
{
  try {
    const bool gui = argc > 1 && std::string(argv[1]) == "--gui";
    os::SystemRef system = os::System::make();
    const char* args[] = { argv[0], "--batch" };
    app::AppOptions options(gui ? 1 : 2, args);
    app::App app;
    check(app.initialize(options) == 0, "Aseprite initialization failed");
    if (gui)
      ui::Manager::getDefault()->dispatchMessages();
    app::oldking::CustomToolbarButtons buttons;
    try {
      testLayout(buttons);
      testCommands(app, buttons);
      if (gui)
        testGui(app, buttons);
    }
    catch (...) {
      if (gui) {
        ui::Manager::getDefault()->dispatchMessages();
        app.run(false);
      }
      throw;
    }
    if (gui) {
      ui::Manager::getDefault()->dispatchMessages();
      app.run(false);
    }
    std::cout << "PASS: " << checks << " checks, " << buttons.count()
              << " installed plugin commands, " << (gui ? "GUI runtime" : "batch runtime")
              << std::endl;
    return 0;
  }
  catch (const std::exception& e) {
    std::cerr << "FAIL after " << checks << " checks: " << e.what() << std::endl;
    return 1;
  }
}
