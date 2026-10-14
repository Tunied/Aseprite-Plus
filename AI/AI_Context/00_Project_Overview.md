# Aseprite Plus

## 项目
基于官方 Aseprite C++ 源码（https://github.com/aseprite/aseprite）的本地增强版本，目标平台为 macOS arm64；
绘图引擎、文件格式、Lua Extension 等全部沿用官方实现，只在官方代码之上做最小改动的增强。

## Remote
- `upstream`：https://github.com/aseprite/aseprite —— 同步来源，会不定期合并进来。
- `origin`：https://github.com/Tunied/Aseprite-Plus —— 初始 fork 来源。

## 关键目录
- `src/app/ui/toolbar.h/.cpp`：原生 Toolbar 的唯一鼠标、Popup、Tooltip 状态机。
- `src/app/ui/oldking/`：OldKing Custom Toolbar 数据和辅助逻辑。
- `src/app/script/plugin_class.cpp`：Lua Extension Command 注册与回调。
- `data/extensions/aseprite-theme/`：原生皮肤和工具图标。
- `src/main/osx/`：macOS bundle 的 Info.plist 与图标资源。
- `build/`：Release / Ninja / Skia arm64 构建目录，保留配置及依赖。

## Aseprite Plus 品牌
- App 名称、bundle identifier、图标全部集中在根 `CMakeLists.txt`：
  `ASEPRITE_PLUS_NAME` / `ASEPRITE_PLUS_IDENTIFIER` / `ASEPRITE_PLUS_ICON`，改一处即可整体改名。
- 图标来源：`src/main/osx/AsepritePlus.png` → `AsepritePlus.icns`，
  换图标后运行 `misc/make_app_icon.sh` 重新生成。
- 为了符合原版 EULA，程序内部的关于窗口、版本号仍保留上游 Aseprite 标识。

## 协作与构建
- 双击父目录 `../一键编译Aseprite.command`：编译并直接安装到 `/Applications/Aseprite Plus.app`（同名替换）。
- 也可以直接用 `ninja -C build aseprite`。
- 上游同步：`git fetch upstream` 后 merge/rebase，再 `git submodule update --init --recursive`；
  重编译前建议先看一眼 build 目录是否需要 `--clean`（上游常更新 laf）。
- 同步时冲突重点为 `src/app/CMakeLists.txt`、`src/app/ui/toolbar.h/.cpp`；增强部分的注册数据都在 `src/app/ui/oldking/` 自有目录。
- 仅按任务读取相关代码，自动验证优先；未实际验收的 GUI 行为不得标为通过。
