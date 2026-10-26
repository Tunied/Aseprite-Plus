// Aseprite Plus. Distributed under the End-User License Agreement for Aseprite.
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/app.h"
#include "app/context.h"
#include "app/doc.h"
#include "app/pref/preferences.h"
#include "app/snap_to_grid.h"
#include "app/tools/active_tool.h"
#include "app/tools/tool_box.h"
#include "app/ui/context_bar.h"
#include "app/ui/editor/editor.h"
#include "app/ui/status_bar.h"
#include "app/ui_context.h"
#include "doc/brush.h"

namespace app {
class CloneStampSourceCommand : public Command {
public:
  CloneStampSourceCommand() : Command(CommandId::CloneStampSource()) {}
protected:
  bool onEnabled(Context* context) override {
    if (!context->isUIAvailable() || !context->activeDocument()) return false;
    auto* editor = UIContext::instance()->getEditorFor(context->activeDocument());
    return editor && !editor->hasCapture();
  }
  std::string onGetFriendlyName() const override { return "Set Clone Stamp Source"; }
  void onExecute(Context* context) override {
    auto* document = context->activeDocument();
    auto* editor = UIContext::instance()->getEditorFor(document);
    const auto editors = UIContext::instance()->getAllEditorsIncludingPreview(document);
    for (auto* view : editors) {
      if (view->hasMouse()) {
        editor = view;
        break;
      }
    }
    App::instance()->activeToolManager()->setSelectedTool(App::instance()->toolBox()->getToolById("clone_stamp"));
    std::optional<gfx::Point> destination;
    if (editor->hasMouse()) {
      auto point = editor->screenToEditor(editor->mousePosInDisplay());
      if (Preferences::instance().cursor.snapToGrid() && editor->docPref().grid.snap()) {
        const auto brush = App::instance()->contextBar()->activeBrush(editor->getCurrentEditorTool());
        const auto size = brush->bounds().size();
        const gfx::Point center(size.w / 2, size.h / 2);
        point = snap_to_grid(editor->docPref().grid.bounds(), point - center,
                             PreferSnapTo::ClosestGridVertex) + center;
      }
      destination = point - editor->mainTilePosition();
    }
    document->cloneStamp().requestSample(destination);
    for (auto* view : editors) view->invalidate();
    if (editor->hasMouse()) editor->showBrushPreview(editor->mousePosInDisplay());
    StatusBar::instance()->showTip(3000, "Clone Stamp: click a source pixel; Esc cancels");
  }
};
Command* CommandFactory::createCloneStampSourceCommand() { return new CloneStampSourceCommand; }
} // namespace app
