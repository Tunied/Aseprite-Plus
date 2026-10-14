// Aseprite OldKing
// Distributed under the End-User License Agreement for Aseprite.

#include "app/ui/oldking/custom_toolbar_buttons.h"

namespace app { namespace oldking {

void registerCustomToolbarEntries(CustomToolbarButtons& buttons)
{
  buttons.registerButton({ "flood-select", "CE_ClickFloodFillRect_Open", "magic_wand",
                           "点击洪水选择" });
  buttons.registerButton({ "generate-shape", "CE_GenShape_Open", "rectangle", "生成 Shape" });
  buttons.registerButton({ "dual-grid-export", "OldKing_DualGridMapExport_Open", "slice",
                           "双网格导出" });
  buttons.registerButton({ "extraction-mask-painter", "CE_ExtractionMaskPainter_Open", "pencil",
                           "Extraction Mask Painter" });
}

} } // namespace app::oldking
