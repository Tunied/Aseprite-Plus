// Targeted regression for grid-lock activation in the real editor state machine.
// Uses isolated preferences; no event loop, screenshots, or manual UI acceptance.
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "app/app.h"
#include "app/cli/app_options.h"
#include "app/commands/commands.h"
#include "app/context.h"
#include "app/doc.h"
#include "app/docs.h"
#include "app/pref/preferences.h"
#include "app/ui/button_set.h"
#include "app/ui/context_bar.h"
#include "app/ui/editor/editor.h"
#include "app/ui/editor/editor_state.h"
#include "app/ui/main_window.h"
#include "app/ui_context.h"
#include "doc/cel.h"
#include "doc/image.h"
#include "doc/image_spec.h"
#include "doc/layer.h"
#include "doc/mask.h"
#include "doc/sprite.h"
#include "os/surface.h"
#include "os/system.h"
#include "ui/graphics.h"
#include "ui/message.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace app;
using namespace doc;
namespace {
int checks = 0;
void check(bool value, const char* label)
{
  ++checks;
  if (!value) throw std::runtime_error(label);
}
void activate(ButtonSet* button)
{
  // ItemChange is the exact callback invoked by the native button's onClick().
  button->ItemChange(button->getItem(0));
}
void repaint(Editor* editor)
{
  ui::PaintMessage paint(0, editor->bounds());
  editor->sendMessage(&paint);
}
void exercise(App& app)
{
  auto* sprite = new Sprite(ImageSpec(ColorMode::RGB,32,32));
  // A plain imported image can have no saved grid at all.
  sprite->setGridBounds(gfx::Rect());
  auto* layer = new LayerImage(sprite);
  sprite->root()->addLayer(layer);
  ImageRef pixels(Image::create(IMAGE_RGB,32,32));
  pixels->clear(rgba(100,120,140,255));
  layer->addCel(new Cel(0,pixels));
  auto* document = new Doc(sprite);
  document->setInhibitBackup(true);
  auto& pref = Preferences::instance().document(document);
  pref.show.grid(false);
  pref.grid.bounds(gfx::Rect());
  app.context()->documents().add(document);
  app.context()->setActiveDocument(document);
  app.context()->setActiveLayer(layer);
  auto* editor = Editor::activeEditor();
  check(editor && editor->getState(),"fresh editor has standby state");
  const auto standby = editor->getState();
  ButtonSet* button = nullptr;
  for (auto* widget : app.contextBar()->children())
    if (widget->id()=="grid_lock") button = dynamic_cast<ButtonSet*>(widget);
  check(button != nullptr,"find native lock button");
  check(!button->getItem(0)->isSelected(),"hidden grid starts unpressed");
  activate(button);
  check(editor->getState()==standby,"lock activation must not pop standby state");
  check(pref.show.grid() && document->gridLock().pending,"hidden unconfigured grid enters picking");
  check(button->getItem(0)->isSelected(),"grid visibility selects lock button");
  repaint(editor); // The original crash dereferenced the lost state in grid rendering.
  const auto pick = editor->editorToScreen(gfx::Point(3,3)+editor->mainTilePosition());
  ui::MouseMessage mouse(ui::kMouseDownMessage,ui::PointerType::Mouse,ui::kButtonLeft,
                         ui::kKeyNoneModifier,pick);
  editor->getState()->onMouseDown(editor,&mouse);
  check(document->gridLock().locked(),"pick uses fallback grid for an unconfigured image");
  check(editor->getState()==standby,"picking consumes click without starting a stroke");
  repaint(editor);
  activate(button);
  check(editor->getState()==standby && !document->gridLock().enabled() && !pref.show.grid(),
        "unlock preserves standby state");
  // Menu-driven visibility remains synchronized with the toolbar.
  auto* context=UIContext::instance();
  auto* show=Commands::instance()->byId(CommandId::ShowGrid());
  context->executeCommand(show);
  check(button->getItem(0)->isSelected(),"menu Show Grid presses button");
  activate(button);
  check(editor->getState()==standby && document->gridLock().pending,
        "already-visible grid also enters picking safely");
  context->executeCommand(show);
  check(!document->gridLock().enabled() && !button->getItem(0)->isSelected(),
        "menu hiding grid cancels picking");
  // The original reason for dropping pixels remains covered: a real floating
  // selection must finish before the new editing constraint is activated.
  Mask selection; selection.replace(gfx::Rect(0,0,8,8));
  document->setMask(&selection); document->setMaskVisible(true);
  editor->startSelectionTransformation(gfx::Point(1,0),0.0);
  check(editor->isMovingPixels(),"start real floating selection");
  activate(button);
  check(editor->getState()==standby && !editor->isMovingPixels() && document->gridLock().pending,
        "activation finishes floating pixels and retains base state");
  repaint(editor);
  activate(button);
  // App::run(false) stops recovery workers before destroying documents.
  // Never delete a GUI document while the backup observer still owns it.
  std::cout << "Grid lock activation assertions complete: " << checks << std::endl;
}
}
int app_main(int argc,char** argv)
{
  try {
    check(std::getenv("ASEPRITE_USER_FOLDER")!=nullptr,"isolated preferences required");
    auto system=os::System::make();
    const char* args[]={argv[0]};
    AppOptions options(1,args);
    App app;
    check(app.initialize(options)==0,"initialize real editor");
    try { exercise(app); }
    catch (...) { app.run(false); throw; }
    app.run(false); // Performs normal document/window cleanup without the event loop.
    std::cout << "Grid lock UI activation: " << checks << " checks passed\n";
    return 0;
  }
  catch(const std::exception& e) { std::cerr << "FAILED: " << e.what() << '\n'; return 1; }
}
