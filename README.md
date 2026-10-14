# Aseprite Plus

> **本 App 面向静态像素画，不提供帧动画编辑功能。** 界面只保留图层工作流，动画相关入口已隐藏。
> 这是个人维护的增强版本，使用前请备份工程，并先用副本确认功能符合预期，以免异常操作或程序问题造成数据损失。
> 不承诺动画工程或旧数据兼容；需要动画编辑时，请使用官方 Aseprite。

> 🇨🇳 中文说明 / Chinese

## 这是什么

**Aseprite Plus** 是在原版 [Aseprite](https://github.com/aseprite/aseprite) 源码基础上做少量增强的个人自用版本，
不是官方发行版，也与原作者 Igara Studio 无关。核心绘图引擎、格式支持、Lua 脚本 API 等全部沿用官方实现。

本仓库长期保持与原版同步：**上游 `aseprite/aseprite` 的新提交会不定期合并进来**，
增强功能在此基础上叠加，不会因为长期分叉而与官方版本脱节。

上游关系：

| remote | 仓库 | 说明 |
|---|---|---|
| `upstream` | https://github.com/aseprite/aseprite | 官方原版，作为同步来源 |
| `origin` | https://github.com/Tunied/Aseprite-Plus | 初始 fork 来源 |

## 当前新增：像素仿制图章

右侧工具栏新增独立的 **仿制图章** 工具，采用硬边像素笔刷，不提供软边硬度或独立流量。

1. 点击图章按钮；按一下 **Y**（或顶部“取样”按钮），再点击画布设置取样点。
2. 在目标位置按住左键拖动，复制源区域像素；再次按 Y 可重新取样，Esc 取消等待取样。
3. 画布上的高对比十字标记显示实际取样位置，目标笔刷轮廓显示覆盖范围。标记不会写入图片或导出结果。
4. 顶部可调整硬边笔刷形状、大小、不透明度、对齐，以及“取样图层 / 取样图层及以下 / 所有可见图层”。

对齐开启时跨笔保留源与目标的相对偏移；关闭时每笔从原取样点重新开始。每笔支持撤销/重做，并遵守目标选区。
取样图层由设置源点时的图层确定，可切到另一个普通图层（包括空图层）绘制；每笔获取新的源快照，避免同笔读到刚写入的像素。
透明源像素和超出源画布的像素会跳过。支持 RGB、灰度和索引色；100% 不透明度下通常直接复制索引；背景保留索引写入透明层时映射为不透明颜色。
图章暂不支持 Tilemap/参考层作为取样或绘制层，也不提供镜像、笔刷动态、源旋转缩放或跨文档取样。
默认单独 Y 用于取样，Cmd/Ctrl+Y 的重做和 Space+Y 的笔刷翻转仍保留；自定义快捷键可以在设置中调整。

## 静态图层界面

图层面板保留选择、显隐、锁定、分组和排序，隐藏帧网格、帧标签、播放控件、洋葱皮、帧菜单及状态栏帧操作。
原时间轴按钮现在切换图层面板显示。动画命令的默认与用户快捷键不再载入，界面命令分发也阻止动画操作。
上游动画实现和文件格式代码仍保留；Lua/旧扩展 API 并未整体删除，但不作为本 App 的动画编辑能力提供或验证。

## 相对原版的变化

1. **macOS App 改名为 Aseprite Plus**
   - App 名称：`Aseprite Plus`，bundle identifier：`studio.oldking.aseprite-plus`，
     与官方版共存互不干扰（原版 `/Applications/Aseprite.app` 不会被覆盖）。
   - 专属应用图标：`src/main/osx/AsepritePlus.png` → `AsepritePlus.icns`；
     同时把上游一直缺失的文件类型图标补齐——`.ase`/`.aseprite` 显示 `Document.icns`、
     `.aseprite-extension` 显示 `Extension.icns`（源图取自 `data/icons/`）。
     换图标后重跑 `misc/make_app_icon.sh` 即可全部重新生成。
   - 品牌相关的构建参数集中在根 `CMakeLists.txt`：`ASEPRITE_PLUS_NAME` /
     `ASEPRITE_PLUS_IDENTIFIER` / `ASEPRITE_PLUS_ICON`，改一处即可整体改名。

2. **原生自定义工具栏（OldKing Custom Toolbar）**
   - 代码位于 `src/app/ui/oldking/`，在原生 `ToolBar` 上扩展出一组固定 Action Button，
     点击时执行已安装 Lua 扩展里对应的 Command。
   - 按钮顺序、命令 ID、图标的唯一注册入口是
     `src/app/ui/oldking/custom_toolbar_entries.cpp`，新增静态按钮只需加一条 `registerButton`。
   - 详细说明见 `AI/AI_Context/CustomToolbar.md`。
   - 回归测试开关 `OLDKING_BUILD_TOOLBAR_TESTS`（默认 OFF），开启后会编译
     `oldking_toolbar_tests`，不需要额外测试依赖。

3. **界面文案微调**（合并自 `Tunied/Aseprite-Plus`）
   - `Crop` / `Trim` 在英文界面中显示为更友好的名字（改动位于 `data/strings/en.ini`）。

> 注意：程序内部的关于窗口、版本号等仍显示上游的 Aseprite 标识——这是有意保留的，
> 以符合原版 [EULA](EULA.txt) 的要求，改动包括包名、图标、静态图层界面与上述功能增强。

## 构建（macOS）

双击与本仓库同级的 `一键编译Aseprite.command` 即可：

1. 自动准备 cmake / ninja / Skia（`aseprite-m124`，匹配 v1.3.18.x）；
2. 用 CMake + Ninja 编译本仓库源码；
3. 把生成的 **Aseprite Plus.app** 安装到 `/Applications`，同名旧版本会被自动替换。

产物：`/Applications/Aseprite Plus.app`（ad-hoc 签名，本机可直接运行）。

手动编译也可以用同一套 CMake 参数：

```sh
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLAF_BACKEND=skia \
  -DSKIA_DIR=$HOME/deps/skia \
  -DSKIA_LIBRARY_DIR=$HOME/deps/skia/out/Release-arm64 \
  -DSKIA_LIBRARY=$HOME/deps/skia/out/Release-arm64/libskia.a \
  -DPNG_ARM_NEON:STRING=on \
  -DASEPRITE_PLUS_NAME:STRING="Aseprite Plus"
cmake --build build --target aseprite
```

Windows / Linux 不受 `ASEPRITE_PLUS_NAME` 影响，仍产出上游默认的 `aseprite` 可执行文件。

## 上游同步流程

双击与本仓库同级的 `一键同步上游Aseprite.command`：它会 fetch 上游、报告落后/领先情况，
能安全快进时自动合并并更新子模块，最后询问是否立即重新编译；有冲突或工作区不干净时
会停下来给出手动命令，绝不 force、不丢弃本地改动。

手动同步也可以：

```sh
git fetch upstream
git merge upstream/main        # 或按照自己的节奏 rebase
git submodule update --init --recursive   # 上游常会更新 laf 等子模块
```

冲突通常集中在 `src/app/CMakeLists.txt` 和 `src/app/ui/toolbar.h/.cpp`，
自定义工具栏的注册数据都在 `src/app/ui/oldking/` 自有目录里，不会和上游文件正面打架。

上游要求的 Skia 版本声明在 `laf/misc/skia-tag.txt`，一键编译脚本会自动读取并跟随，
同步后即使 Skia 大版本变化也不需要改脚本（新版会下载到独立的 `~/deps/skia-<tag>` 目录）。

## 许可证

与原版一致，见下方 License 章节。本仓库只做增强，不修改也不重新解释原版授权条款。

---

# Aseprite

[![build](https://github.com/aseprite/aseprite/actions/workflows/build.yml/badge.svg)](https://github.com/aseprite/aseprite/actions/workflows/build.yml)
[![Translation Status](https://hosted.weblate.org/widget/aseprite/aseprite/svg-badge.svg)](https://hosted.weblate.org/engage/aseprite/)
[![Discourse Community](https://img.shields.io/badge/discourse-community-brightgreen.svg?style=flat)](https://community.aseprite.org/)
[![Discord Server](https://discordapp.com/api/guilds/324979738533822464/embed.png)](https://discord.gg/Yb2CeX8)

## Introduction

**Aseprite** is a program to create animated sprites. Its main features are:

* Sprites are composed of [layers &amp; frames](https://www.aseprite.org/docs/timeline/) as separated concepts.
* Support for [color profiles](https://www.aseprite.org/docs/color-profile/) and different [color modes](https://www.aseprite.org/docs/color-mode/): RGBA, Indexed (palettes up to 256 colors), Grayscale.
* [Animation facilities](https://www.aseprite.org/docs/animation/), with real-time [preview](https://www.aseprite.org/docs/preview-window/) and [onion skinning](https://www.aseprite.org/docs/onion-skinning/).
* [Export/import](https://www.aseprite.org/docs/exporting/) animations to/from [sprite sheets](https://www.aseprite.org/docs/sprite-sheet/), GIF files, or sequence of PNG files (and FLC, FLI, JPG, BMP, PCX, TGA).
* [Multiple editors](https://www.aseprite.org/docs/workspace/#drag-and-drop-tabs) support.
* [Layer groups](https://imgur.com/x3OKkGj) for organizing your work, and [reference layers](https://twitter.com/aseprite/status/806889204601016325) for rotoscoping.
* Pixel-art specific tools like [Pixel Perfect freehand mode](https://imgur.com/0fdlNau), [Shading ink](https://www.aseprite.org/docs/shading/), [Custom Brushes](https://twitter.com/aseprite/status/1196883990080344067), [Outlines](https://twitter.com/aseprite/status/1126548469865431041), [Wide Pixels](https://imgur.com/1yZKUcs), etc.
* Other special drawing tools like [Pressure sensitivity](https://twitter.com/aseprite/status/1253770784708886533), [Symmetry Tool](https://twitter.com/aseprite/status/659709226747625472), [Stroke and Fill](https://imgur.com/7JZQ81o) selection, [Gradients](https://twitter.com/aseprite/status/1126549217856622597).
* [Tiled mode](https://youtu.be/G_JeWBaxQIg) useful to draw patterns and textures.
* [Transform multiple frames/layers](https://twitter.com/aseprite/status/1170007034651172866) at the same time.
* [Lua scripting capabilities](https://www.aseprite.org/docs/scripting/).
* [CLI - Command Line Interface](https://www.aseprite.org/docs/cli/) to automatize tasks.
* [Quick Reference / Cheat Sheet](https://www.aseprite.org/quickref/) keyboard shortcuts ([customizable keys](https://imgur.com/rvAUxyF) and [mouse wheel](https://imgur.com/oNqFqVb)).
* [Reopen closed files](https://twitter.com/aseprite/status/1202641475256881153) and [recover data](https://www.aseprite.org/docs/data-recovery/) in case of crash.
* Undo/Redo for every operation and support for [non-linear undo](https://imgur.com/9I42fZK).
* [More features &amp; tips](https://twitter.com/aseprite/status/1124442198651678720)

## Issues

There is a list of
[Known Issues](https://github.com/aseprite/aseprite/issues) (things
to be fixed or that aren't yet implemented).

If you found a bug or have a new idea/feature for the program,
[you can report them](https://github.com/aseprite/aseprite/issues/new).

## Support

You can ask for help in:

* [Aseprite Community](https://community.aseprite.org/)
* [Aseprite Discord Server](https://discord.gg/Yb2CeX8)
* Official support: [support@aseprite.org](mailto:support@aseprite.org)
* Social networks and community-driven places:
  [Twitter](https://twitter.com/aseprite/),
  [Facebook](https://facebook.com/aseprite/),
  [YouTube](https://www.youtube.com/user/aseprite),
  [Instagram](https://www.instagram.com/aseprite/).

## Credits

Aseprite was originally created by [David Capello](https://davidcapello.com/)
and is now being developed and maintained by [Igara Studio](https://igara.com/)
and contributors.

Check the [AUTHORS](AUTHORS.md) file for details about the active team
of developers working on Aseprite.

## License

This program is distributed under three different licenses:

1. Source code and official releases/binaries are distributed under
   our [End-User License Agreement for Aseprite (EULA)](EULA.txt). Please check
   that there are [modules/libraries in the source code](src/README.md) that
   are distributed under the MIT license
   (e.g. [laf](https://github.com/aseprite/laf),
   [clip](https://github.com/aseprite/clip),
   [undo](https://github.com/aseprite/undo),
   [observable](https://github.com/aseprite/observable),
   [ui](src/ui), etc.).
2. You can request a special
   [educational license](https://www.aseprite.org/faq/#is-there-an-educational-license)
   in case you are a teacher in an educational institution and want to
   use Aseprite in your classroom (in-situ).
3. Steam releases are distributed under the terms of the
   [Steam Subscriber Agreement](http://store.steampowered.com/subscriber_agreement/).

You can get more information about Aseprite license in the
[FAQ](https://www.aseprite.org/faq/#licensing-&-commercial).
