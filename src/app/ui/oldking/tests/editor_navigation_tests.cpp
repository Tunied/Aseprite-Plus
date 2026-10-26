// Headless checks for edge-scroll precision and persisted navigation preferences.
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "app/app.h"
#include "app/cli/app_options.h"
#include "app/commands/move_thing.h"
#include "app/commands/params.h"
#include "app/ini_file.h"
#include "app/pref/preferences.h"
#include "app/ui/editor/edge_scroll.h"
#include "os/system.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int checks = 0;
void check(bool value, const char* label)
{
  ++checks;
  if (!value)
    throw std::runtime_error(label);
}

void scrollChecks()
{
  app::EdgeScroll scroll;
  for (int sign : {-1, 1}) {
    for (int percent : {1, 25, 37, 100}) {
      scroll.reset();
      gfx::Point total(0, 0);
      for (int i = 0; i < 100; ++i)
        total += scroll.scale(gfx::Point(sign, -sign), percent);
      check(total == gfx::Point(sign * percent, -sign * percent),
            "small mouse events retain fractional movement in all directions");
    }
  }
  scroll.reset();
  check(scroll.scale({40, -80}, 25) == gfx::Point(10, -20), "large diagonal event is reduced");
  scroll.reset();
  check(scroll.scale({3, -3}, 25) == gfx::Point(0, 0), "fraction stays below a screen pixel");
  check(scroll.scale({-4, 4}, 25) == gfx::Point(-1, 1), "direction reversal has no stale fraction");
  scroll.reset();
  scroll.scale({3, 3}, 25);
  check(scroll.scale({0, 1}, 25) == gfx::Point(0, 1), "separate vertical event retains fractions");
  check(scroll.scale({1, 0}, 25) == gfx::Point(1, 0), "separate horizontal event retains fractions");
  scroll.scale({2, 2}, 25);
  scroll.reset();
  check(scroll.scale({1, 1}, 25) == gfx::Point(0, 0), "new drag discards old fractions");
  scroll.reset();
  check(scroll.scale({100, -100}, -50) == gfx::Point(1, -1), "invalid low speed is clamped");
  scroll.reset();
  check(scroll.scale({9, -9}, 500) == gfx::Point(9, -9), "invalid high speed is clamped");
}
} // namespace

int app_main(int argc, char** argv)
{
  try {
    check(std::getenv("ASEPRITE_USER_FOLDER") != nullptr, "Use isolated preferences");
    auto system = os::System::make();
    const char* args[] = {argv[0], "--batch"};
    app::AppOptions options(2, args);
    app::App application;
    check(application.initialize(options) == 0, "batch initialization");
    auto& pref = app::Preferences::instance();
    const bool verifySaved = argc > 1 && std::string(argv[1]) == "--verify-saved";
    check(pref.editor.autoScrollSpeed() == (verifySaved ? 17 : 25), "edge speed default/persistence");
    check(pref.selection.shiftMoveStep() == (verifySaved ? 7 : 4), "selection step default/persistence");
    if (!verifySaved) {
      scrollChecks();
      app::MoveThing move;
      move.onLoadParams({{"units", "selection-step"}, {"direction", "left"}, {"quantity", "1"}});
      check(move.units == app::MoveThing::SelectionStep && move.quantity == 1 &&
              move.direction == app::MoveThing::Left, "configured step parses as a distinct unit");
      move.onLoadParams({{"units", "tile-width"}, {"quantity", "1"}});
      check(move.units == app::MoveThing::TileWidth, "explicit tile commands remain available");
      move.onLoadParams({{"units", "pixel"}, {"quantity", "1"}});
      check(move.units == app::MoveThing::Pixel, "ordinary arrow movement keeps pixel units");
      pref.editor.autoScrollSpeed(17);
      pref.selection.shiftMoveStep(7);
      pref.save();
      app::flush_config_file();
    }
    std::cout << "PASS: " << checks << " editor navigation checks (headless)\n";
    return 0;
  }
  catch (const std::exception& e) {
    std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
    return 1;
  }
}
