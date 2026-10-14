# Aseprite Plus 项目长期记忆

## 定位
- 基于官方 `aseprite/aseprite`（remote `upstream`）的个人增强版；`origin` 指向
  `Tunied/Aseprite-Plus`（初始 fork 来源）。会不定期 merge/rebase 上游。
- 目标平台 macOS arm64；Skia `m124`（匹配 v1.3.18.x），依赖装在 `~/deps/skia`、`~/deps/tools`。

## 构建与安装
- 双击同级 `../一键编译Aseprite.command`：编译本仓库并安装到 `/Applications/Aseprite Plus.app`，
  同名替换（旧包先进废纸篓）。支持 `--clean / --jobs=N / --debug-info / --install-dir=DIR`。
- 双击同级 `../一键同步上游Aseprite.command`：fetch 上游、可快进才自动合并、同步子模块，
  冲突/脏工作区时停下给手动命令，不 force。
- 增量编译也可 `ninja -C build aseprite`；产物在 `build/bin/Aseprite Plus.app`。
- **安装时不要递归删整个 .app**：用 mv 到 `~/.Trash` + mv staging 两步 rename。
- Skia 版本从 `laf/misc/skia-tag.txt` 自动读取并跟随上游；版本变化装到独立
  `~/deps/skia-<tag>` 目录（标记文件 `~/deps/skia/.plus-skia-tag`）。

## 品牌相关的三处真源
- 根 `CMakeLists.txt`：`ASEPRITE_PLUS_NAME`（App 名，可含空格）、`ASEPRITE_PLUS_IDENTIFIER`、
  `ASEPRITE_PLUS_ICON`。
- `src/CMakeLists.txt`：`OUTPUT_NAME` = bundle 目录名与可执行文件名；
  `DATA_OUTPUT_DIR` / symlink 统一用 `ASEPRITE_APP_BUNDLE_DIR` 变量。
- `src/main/osx/Info.plist`：`CFBundleName/DisplayName/Executable/Identifier/IconFile`
  用 CMake 模板变量。官方那 3 处 `bin/Aseprite.app` 硬编码是笔误，别照抄回来。
- 图标：`src/main/osx/AsepritePlus.png` → `misc/make_app_icon.sh` → `AsepritePlus.icns`。

## 自有功能
- OldKing 自定义工具栏：`src/app/ui/oldking/`，注册入口 `custom_toolbar_entries.cpp`，
  测试开关 `OLDKING_BUILD_TOOLBAR_TESTS`（默认 OFF）。
- 合并自 Tunied fork：英文文案 `Cr&op to Selection` / `&Trim Unused Pixels`（`data/strings/en.ini`）。

## 边界
- 不代替用户 git 提交；改动按用户需求落到工作区由用户自行 commit（见代码 `git status`）。
- GUI、视觉、真实交互由用户验收；自动验证只覆盖编译、安装、签名与 `--version` 自检。
