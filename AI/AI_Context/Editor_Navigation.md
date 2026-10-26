# 编辑器边缘滚动与选区步长

## 当前行为
- Preferences → Editor → Auto-scroll on editor edges 控制原生边缘滚动开关。
- 同页 Edge auto-scroll speed 保存为 `editor.auto_scroll_speed`，默认 25，范围 1–100，单位为原版边缘滚动速度的百分比。
- 该速度作用于 `AutoScroll::MouseDir`（选区拖动、浮动选区、绘图等）；手形工具 `ScrollDir` 保持原生速度。
- `EdgeScroll` 在屏幕像素层面累计百分比余量，避免低速小事件取整后完全不动；进入视口、新鼠标按下、禁用自动滚动时重置，单轴反向时丢弃该轴余量，零位移保留余量以兼容分轴鼠标事件。鼠标回绕仍使用原始位移，不受速度比例影响。
- Preferences → Selection → Shift + Arrow move step 保存为 `selection.shift_move_step`，默认 4，范围 2–64，单位是图像像素。
- 默认 Shift+方向键使用 `MoveMask` / `units=selection-step` 移动内容；Alt+Shift+方向键以同一步长移动边框。普通方向键保持每次 1 像素；步长不取决于网格尺寸或缩放。
- 显式 `tile-width` / `tile-height` 命令和用户自定义快捷键仍保留其单位语义。偏好设置在 Apply / OK 后保存，运行路径也裁剪非法配置范围。

## 实现与验证入口
- `data/pref.xml`、`data/widgets/options.xml`、`src/app/commands/cmd_options.cpp`：偏好声明与保存。
- `data/gui.xml`、`src/app/commands/move_thing.cpp`：默认快捷键、单位解析与动态步长。
- `src/app/ui/editor/editor.cpp`、`edge_scroll.h`：原生鼠标回绕与低速累计。
- `OLDKING_BUILD_NAVIGATION_TESTS=ON` 构建 `oldking_navigation_tests`；`sh misc/test_editor_navigation.sh` 使用隔离偏好目录，无窗口检查双向/双轴累计、反向/重置、范围裁剪、单位解析及跨进程偏好持久化。
- 编译、无窗口测试不代表 GUI 或拖拽手感验收，真实交互由用户最终确认。
