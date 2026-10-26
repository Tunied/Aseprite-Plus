// Native font/layout checks on memory surfaces, without opening windows.
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "app/app.h"
#include "app/cli/app_options.h"
#include "app/fonts/font_info.h"
#include "app/pref/preferences.h"
#include "app/ui/skin/skin_theme.h"
#include "os/surface.h"
#include "os/system.h"
#include "text/font.h"
#include "text/font_metrics.h"
#include "text/text_blob.h"
#include "ui/button.h"
#include "ui/box.h"
#include "ui/entry.h"
#include "ui/graphics.h"
#include "ui/paint_event.h"
#include "ui/theme.h"
#include "ui/window.h"
#include "ui/system.h"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool value, const char* message)
{
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}

os::SurfaceRef surface()
{
  auto result = os::System::instance()->makeRgbaSurface(1024, 256);
  result->clear();
  return result;
}

int inkPixels(const os::SurfaceRef& image)
{
  os::SurfaceLock lock(image.get());
  int n = 0;
  for (int y = 0; y < image->height(); ++y)
    for (int x = 0; x < image->width(); ++x)
      n += gfx::geta(image->getPixel(x, y)) > 0;
  return n;
}

int referenceInk(const text::FontRef& font, const char* text)
{
  auto image = surface();
  ui::Graphics g(image);
  g.setFont(font);
  g.drawText(text, gfx::rgba(255, 0, 255, 255), gfx::ColorNone, gfx::Point(20, 20));
  return inkPixels(image);
}

void entryInk(app::skin::SkinTheme& theme, const char* text)
{
  ui::Entry entry(64, "%s", text);
  entry.hideCaret();
  entry.setBounds(gfx::Rect(0, 0, 600, entry.sizeHint().h));
  auto full = surface();
  {
    ui::Graphics g(full);
    g.setFont(entry.font());
    ui::PaintEvent ev(&entry, &g);
    theme.paintEntry(ev);
  }
  entry.setText("");
  auto empty = surface();
  {
    ui::Graphics g(empty);
    g.setFont(entry.font());
    ui::PaintEvent ev(&entry, &g);
    theme.paintEntry(ev);
  }
  int changed = 0;
  {
    os::SurfaceLock a(full.get()), b(empty.get());
    for (int y = 0; y < full->height(); ++y)
      for (int x = 0; x < full->width(); ++x)
        changed += full->getPixel(x, y) != empty->getPixel(x, y);
  }
  const int reference = referenceInk(entry.font(), text);
  if (changed != reference) {
    std::cerr << "entry text=" << text << " size=" << entry.font()->size()
              << " bounds=" << entry.bounds().h << " changed=" << changed
              << " reference=" << reference << " baseline=" << entry.textBaseline() << '\n';
  }
  check(changed == reference, "entry clips glyphs");
}

void buttonInk(app::skin::SkinTheme& theme)
{
  ui::HBox row;
  row.setBorder(gfx::Border(0, 0, 0, 2 * ui::guiscale()));
  ui::Button button("Source (Y)");
  row.addChild(&button);
  row.setBounds(gfx::Rect(0, 0, 600,
                         std::max(row.sizeHint().h, theme.dimensions.contextBarHeight())));
  check(button.bounds().h >= button.sizeHint().h, "context bar clips source button");
  ui::Style textStyle(button.style());
  textStyle.layers().clear();
  ui::Style::Layer text;
  text.setType(ui::Style::Layer::Type::kText);
  text.setColor(gfx::rgba(255, 0, 255, 255));
  textStyle.addLayer(text);
  auto image = surface();
  ui::Graphics g(image);
  g.clipRect(button.clientBounds());
  g.setFont(button.font());
  theme.paintWidget(&g, &button, &textStyle, button.clientBounds());
  check(inkPixels(image) == referenceInk(button.font(), "Source (Y)"), "button clips glyphs");
}

void titlebar(app::skin::SkinTheme& theme)
{
  ui::Window window(ui::Window::WithTitleBar, "Preferences");
  window.setBounds(gfx::Rect(0, 0, 600, 160));
  auto image = surface();
  ui::Graphics g(image);
  g.setFont(window.font());
  theme.paintWidget(&g, &window, window.style(), window.clientBounds());
  os::SurfaceLock lock(image.get());
  check(image->getPixel(300, window.border().top() - 3 * ui::guiscale()) ==
          theme.colors.windowTitlebarFace(), "expanded titlebar has an unpainted gap");
  for (auto child : window.children()) {
    if (child->type() == ui::kWindowTitleLabelWidget)
      check(child->bounds().y2() < window.border().top(), "title label overlaps dialog content");
  }
}
} // namespace

int app_main(int argc, char** argv)
{
  try {
    check(std::getenv("ASEPRITE_USER_FOLDER") != nullptr, "Use an isolated test preference directory");
    auto system = os::System::make();
    const char* args[] = {argv[0], "--batch"};
    app::AppOptions options(2, args);
    app::App app;
    check(app.initialize(options) == 0, "batch initialization");
    ui::UISystem uiSystem;
    app::skin::SkinTheme theme;
    auto& pref = app::Preferences::instance();
    const int screenScale = pref.general.screenScale();
    const int uiScale = pref.general.uiScale();
    for (const auto* themeId : {"default", "default-dark"}) {
      pref.theme.selected(themeId);
      for (int scale : {1, 2}) {
        for (int percent : {75, 100, 125, 150, 200, 100}) {
          pref.general.fontScale(percent);
          ui::set_theme(&theme, scale);
          check(pref.theme.selected() == themeId, "requested theme failed to load");
          check(pref.general.screenScale() == screenScale && pref.general.uiScale() == uiScale,
                "font scaling changed screen/UI preferences");
          auto font = theme.getDefaultFont();
          check(std::fabs(font->size() - std::round(12.0f * percent / 100) * scale) < .01f,
                "main font size");
          check(std::fabs(theme.getMiniFont()->size() - std::round(10.0f * percent / 100) * scale) < .01f,
                "mini font size");
          text::FontMetrics metrics;
          font->metrics(&metrics);
          check(std::fabs(metrics.descent - metrics.ascent - font->size() * 4 / 3) < .02f,
                "UI font lost its full original line box");
          const auto titleStyle = theme.styles.windowTitleLabel();
          check(titleStyle->margin().top() + std::ceil(metrics.descent - metrics.ascent) <
                  theme.styles.windowWithTitle()->border().top(), "title overlaps dialog content");
          buttonInk(theme);
          titlebar(theme);
          for (const auto* text : {"29", "100%", "首选项", "Preferences"})
            entryInk(theme, text);
        }
      }
    }
    const app::FontInfo custom(app::FontInfo::Type::Name, "Fusion Pixel", 14.5f);
    pref.theme.font(base::convert_to<std::string>(custom));
    for (int percent : {150, 100}) {
      pref.general.fontScale(percent);
      ui::set_theme(&theme, 1);
      const float expected = percent == 100 ? 14.5f : std::round(14.5f * percent / 100);
      check(std::fabs(theme.getDefaultFont()->size() - expected) < .01f,
            "custom font scaling");
      check(theme.getDefaultFontInfo().size() == 14.5f, "custom base font size was scaled twice");
    }
    auto* fonts = app::Fonts::instance();
    auto normal = fonts->fontByName("Aseprite", 8, 1.0f);
    auto scaled = fonts->fontByName("Aseprite", 8, 1.5f);
    check(normal && normal->fallback() && normal->fallback()->size() == 14,
          "base fallback font size");
    check(scaled && scaled->fallback() && scaled->fallback()->size() == 21,
          "independent fallback font scaling");
    check(normal != scaled && normal->fallback()->size() == 14,
          "font cache mixes fallback scales");
    std::cout << "PASS: " << checks << " typography checks (offscreen; no GUI acceptance)\n";
    return 0;
  }
  catch (const std::exception& e) {
    std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
    return 1;
  }
}
