# 界面字体

## 字体资源与度量
- 内置浅色、深色主题使用 Fusion Pixel：主字体 12px，小字体 10px，禁用抗锯齿和 hinting。
- `data/fonts/fonts.xml` 负责字体与回退声明；字体及 OFL 许可放在 `data/fonts/`。
- 保留字体原始行度量，12px 主字体实际行高约 16px、10px 小字体约 14px；不能把行高硬压到标称字号，否则部分中文会被裁切。控件按完整行高预留空间。

## 独立字体缩放
- Preferences → General → Font Size：75%–200%，默认 100%，Apply / OK 后应用并保存为 `general.font_scale`。
- 不修改 `general.screen_scale` 或 `general.ui_scale`，不改变画布内容、文字工具字号或图标缩放。
- 主字体和小字体均按主题基础字号乘字体比例，并先四舍五入到逻辑像素，再乘 UI Element Scaling；100% 保留原始字号，像素字体在原生字号及整数倍时最清晰。
- Theme 页面自定义字体保留原始字号；Font Size 只在加载界面字体时乘一次，不回写自定义字号。
- 主题标题栏和 Context Bar 按实际行高预留空间；其他控件继续使用原生字号驱动的布局。文字所需的布局空间可以变大，但屏幕和 UI 缩放设置保持不变。
- `FontData` 缓存区分字体尺寸与回退比例，避免 UI 字体、回退字体及未按 UI 缩放的 Canvas 字体串用。

## 代码入口与验证
- `src/app/commands/cmd_options.cpp`、`data/widgets/options.xml`：偏好设置界面和 Apply。
- `src/app/ui/skin/skin_theme.cpp`：主题字号、布局空间及输入框基线。
- `src/app/ui/context_bar.cpp`：工具栏按可见控件的真实尺寸及边距计算高度，主题尺寸为下限。
- `src/ui/widget.cpp`：文字高度向上取整，避免浮点误差截掉一行像素。
- `OLDKING_BUILD_TYPOGRAPHY_TESTS=ON` 启用 `build/bin/oldking_typography_tests`，用隔离的 `ASEPRITE_USER_FOLDER` 执行。使用原生字体和内存画面检查字体比例、明暗主题、按钮和输入框裁字、标题空间；不打开窗口。
- C++ 编译走 ai-queue heavy（默认 `-j2`），局部无窗口检查走 normal；真实视觉与交互仍由用户验收。
