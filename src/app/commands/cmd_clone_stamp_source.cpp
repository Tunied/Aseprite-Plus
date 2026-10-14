// Aseprite Plus. Distributed under the End-User License Agreement for Aseprite.
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/app.h"
#include "app/context.h"
#include "app/doc.h"
#include "app/tools/active_tool.h"
#include "app/tools/tool_box.h"
#include "app/ui/editor/editor.h"
#include "app/ui/status_bar.h"
#include "app/ui_context.h"

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
    App::instance()->activeToolManager()->setSelectedTool(App::instance()->toolBox()->getToolById("clone_stamp"));
    document->cloneStamp().pending = true;
    for (auto* editor : UIContext::instance()->getAllEditorsIncludingPreview(document)) editor->invalidate();
    StatusBar::instance()->showTip(3000, "Clone Stamp: click a source pixel; Esc cancels");
  }
};
Command* CommandFactory::createCloneStampSourceCommand() { return new CloneStampSourceCommand; }
} // namespace app
