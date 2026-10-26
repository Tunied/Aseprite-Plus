# 原生 Custom Toolbar

## 实现边界
- 扩展同一条 `app::ToolBar`；绘图工具下方、Preview/Timeline 上方有四个固定 Action Button。
- ToolBar 独占 Hover、Capture、TipTimer、TipWindow、Popup 状态。CustomToolbarButtons 只持有注册数据，提供 Bounds、HitTest、Command、Tooltip 和绘制辅助。
- `src/app/ui/oldking/custom_toolbar_entries.cpp` 是按钮顺序、命令和图标的唯一注册入口。增加静态按钮只需增加一条 `registerButton`。
- 不注册绘图 Tool，不调用具体 Lua 文件，不实现插件业务，不新增主题资源。

## 当前映射
| 顺序 | 插件 | Command ID | 内置 tool icon ID |
|---|---|---|---|
| 1 点击洪水选择 | ce-click-flood-fill-rect | CE_ClickFloodFillRect_Open | magic_wand |
| 2 生成 Shape | ce-gen-shape | CE_GenShape_Open | rectangle |
| 3 双网格导出 | oldking-dual-grid-map-export | OldKing_DualGridMapExport_Open | slice |
| 4 Extraction Mask Painter | ce-extraction-mask-painter | CE_ExtractionMaskPainter_Open | pencil |

洪水选择绑定面板主入口；其 `FloodFillChoice` 是另一个直接开始选择的 Q 键命令，Toolbar 不替代它。四个主命令没有 onenabled/onchecked 回调；沿用默认 enabled=true、checked=false，插件自己检查文档等业务条件。

## 执行与状态
- Lua `Plugin_newCommand` 注册 PluginCommand 到 Commands registry。
- 每次查询/点击按 ID 延迟解析，成员不缓存 Command 裸指针；插件缺失或被禁用时不可执行，Tooltip 回退到注册名。
- 点击使用 `Context::executeCommandFromMenuOrShortcut`，复用菜单/快捷键执行保护。
- Enabled/Checked 查询先更新 Context flags、加载空 Params；状态来自官方 Command。
- Checked/Hover 使用 `toolbuttonHot`，与 Preview/Timeline 一致；Disabled 使用原生 `colors.disabled()` 给图标着色。
- Tooltip 沿用 ToolBar 的 TipWindow，采用 Command friendlyName 和原生 KeyboardShortcuts 查询；不把另一个 Command 的 Q 键误列为面板入口快捷键。
- 命令执行完成后使 Toolbar 重绘，以刷新状态。

## Index 与布局
- `BaseIndex=-1000`，按钮 b 编码为 `-1000-b`；有效范围由注册数量决定，先识别后进入原生 Bounds/Tooltip 分支。
- 采用内置图标尺寸和 guiscale；垂直步距为 `icon.h-scale`，上下分区留小间距，复用最后按钮皮肤。
- 皮肤边框像素自然重叠；HitTest 倒序保证后来绘制的按钮拥有交叠像素，每次只执行一个 Command。
- 从 Preview 向上布局，Custom 区域始终固定。`m_minHeight` 计入固定区域；高度不足向上取整隐藏普通 ToolGroup，继续使用原生 ToolStrip Overflow。
- sizeHint 保留固定区域和一个 Overflow 按钮所需高度；物理上低于该高度的空间无法容纳所有固定按钮。
- Dock、Tooltip 朝向继续使用原生左右判断。

## 构建与回归
- 普通构建：`ninja -C build aseprite`（或双击父目录 `../一键编译Aseprite.command`）。
- 产物：`build/bin/macos.noindex/Aseprite Plus.app`；唯一正式安装路径为 `/Applications/Aseprite Plus.app`，覆盖安装、不保留旧 App。安装入口和搜索去重规则见 Overview。
  `build/bin/aseprite` 是 bundle 中可执行文件的符号链接。
- `OLDKING_BUILD_TOOLBAR_TESTS` 是默认 OFF 的局部 CMake 开关；启用后编译 `oldking_toolbar_tests`，无需新增测试依赖。
- 测试前建立独立 `ASEPRITE_USER_FOLDER`，复制实际已安装四插件及 Super Menu；禁止直接用真实用户目录执行测试，因为测试会增加临时快捷键并调整布局。
- `build/bin/oldking_toolbar_tests` 检查 batch registry/dispatch；加 `--gui` 用真实 Aseprite GUI 对象检查事件、插件 Dialog、皮肤、Dock、Popup 和布局。不进行导出或其他插件业务操作。
- GUI 回归通过程序发送事件并检查状态/像素；最终视觉、真实鼠标操作手感及插件完整业务验收由用户完成。

## 上游维护
`git fetch upstream` 后 merge/rebase 到当前分支，再同步 submodule 并重编译。冲突重点为
`toolbar.h/.cpp`、`src/app/CMakeLists.txt`；OldKing 注册数据集中在自有目录。
注意上游改动若波及根 `CMakeLists.txt` 或 `src/CMakeLists.txt`，要保留 Aseprite Plus 的品牌段落
（`ASEPRITE_PLUS_NAME` 等变量、`OUTPUT_NAME`、`DATA_OUTPUT_DIR` 与 Info.plist 资源不能用官方写法覆盖）。
