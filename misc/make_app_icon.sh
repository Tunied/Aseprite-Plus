#!/bin/bash
#
# 生成 Aseprite Plus 的 macOS 图标（.icns）
# ---------------------------------------------------------------------------
# 用法：  ./misc/make_app_icon.sh [应用图标源图 PNG]
# 默认应用图标源图： src/main/osx/AsepritePlus.png
#
# 会生成三个图标（均放进 src/main/osx/，由 CMake 打进 bundle 的 Contents/Resources）：
#   AsepritePlus.icns  —— App 本体图标
#   Document.icns      —— .ase / .aseprite 等精灵文件图标（源图 data/icons/ase256.png）
#   Extension.icns     —— .aseprite-extension 扩展文件图标（源图 data/icons/ext256.png）
#
# 依赖 macOS 自带的 sips / iconutil，无需额外安装。
# 应用图标源图建议为正方形且至少 1024x1024。
# ---------------------------------------------------------------------------

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST_DIR="${REPO_DIR}/src/main/osx"
ICONSET="${REPO_DIR}/build/AsepritePlus.iconset"
SRC="${1:-${DEST_DIR}/AsepritePlus.png}"

check_source() {
  local f="$1"
  [ -f "$f" ] || { echo "找不到源图标：${f}" >&2; exit 1; }
  local w h
  w="$(/usr/bin/sips -g pixelWidth "$f" | awk '/pixelWidth:/{print $2}')"
  h="$(/usr/bin/sips -g pixelHeight "$f" | awk '/pixelHeight:/{print $2}')"
  echo "  源图 ${f} (${w}x${h})"
  [ "$w" -ge 256 ] || { echo "源图小于 256，产物会比较糊" >&2; exit 1; }
}

make_icns() {
  local src="$1" out="$2"
  rm -rf "${ICONSET}"
  mkdir -p "${ICONSET}"
  local name pixels
  for pair in \
    "icon_16x16.png:16"       "icon_16x16@2x.png:32" \
    "icon_32x32.png:32"       "icon_32x32@2x.png:64" \
    "icon_128x128.png:128"    "icon_128x128@2x.png:256" \
    "icon_256x256.png:256"    "icon_256x256@2x.png:512" \
    "icon_512x512.png:512"    "icon_512x512@2x.png:1024"
  do
    name="${pair%%:*}"; pixels="${pair##*:}"
    /usr/bin/sips -z "${pixels}" "${pixels}" "${src}" --out "${ICONSET}/${name}" >/dev/null
  done
  rm -f "${out}"
  /usr/bin/iconutil -c icns "${ICONSET}" -o "${out}"
  echo "  已生成 ${out}"
}

echo "== Aseprite Plus 应用图标 =="
check_source "${SRC}"
make_icns "${SRC}" "${DEST_DIR}/AsepritePlus.icns"

echo "== 精灵文件图标 (Document.icns) =="
check_source "${REPO_DIR}/data/icons/ase256.png"
make_icns "${REPO_DIR}/data/icons/ase256.png" "${DEST_DIR}/Document.icns"

echo "== 扩展文件图标 (Extension.icns) =="
check_source "${REPO_DIR}/data/icons/ext256.png"
make_icns "${REPO_DIR}/data/icons/ext256.png" "${DEST_DIR}/Extension.icns"

rm -rf "${ICONSET}"
echo "完成。重新编译后生效。"
