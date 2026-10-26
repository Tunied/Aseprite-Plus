#!/bin/bash
# Install the current build at the one canonical app path, without app backups.
set -euo pipefail
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${1:-"$project_dir/build"}
app_name=$(sed -n 's/^ASEPRITE_PLUS_NAME:STRING=//p' "$build_dir/CMakeCache.txt")
case "$app_name" in ''|*/*|.|..) printf 'Invalid app name\n' >&2; exit 1;; esac
built="$build_dir/bin/macos.noindex/$app_name.app"
installed="/Applications/$app_name.app"
plist="$built/Contents/Info.plist"
bundle_id=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$plist")
exec_name=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$plist")
lsregister='/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister'

remove_app() {
  # Refuse symlinks and unexpected bundles before removing an app directory.
  python3 - "$1" "$bundle_id" "$installed" "$build_dir/bin/$app_name.app" "$exec_name" <<'PY'
import pathlib, plistlib, shutil, sys
path = pathlib.Path(sys.argv[1])
if path.is_symlink():
    raise SystemExit(f'Refusing to remove symlink: {path}')
if not path.exists():
    raise SystemExit(0)
plist = path / 'Contents/Info.plist'
if plist.is_file():
    recognized = plistlib.loads(plist.read_bytes()).get('CFBundleIdentifier') == sys.argv[2]
else:
    # CMake may have removed the old generated plist when relocating output.
    recognized = (path.absolute() in (pathlib.Path(sys.argv[3]).absolute(),
                                     pathlib.Path(sys.argv[4]).absolute()) and
                  (path / 'Contents/MacOS' / sys.argv[5]).is_file())
if not recognized:
    raise SystemExit(f'Refusing to remove a different app: {path}')
shutil.rmtree(path)
PY
}

if [ ! -w /Applications ]; then
  printf 'Cannot update %s: /Applications is not writable\n' "$installed" >&2
  exit 1
fi

# .noindex also hides the temporary staged bundle from app search.
staging_parent='/Applications/.aseprite-plus-install.noindex'
mkdir -p "$staging_parent"
staging_root=$(mktemp -d "$staging_parent/stage.XXXXXX")
staged="$staging_root/$app_name.app"
/usr/bin/ditto "$built" "$staged"
/usr/bin/xattr -cr "$staged"
/usr/bin/codesign --force --sign - "$staged"
/usr/bin/codesign --verify --strict "$staged"

# Validate the replacement before deleting the old app. No renamed/trash copy.
"$lsregister" -u "$installed" >/dev/null 2>&1 || true
remove_app "$installed"
mv -- "$staged" "$installed"
rmdir "$staging_root"
rmdir "$staging_parent" 2>/dev/null || true
/usr/bin/codesign --verify --strict "$installed"

# Remove only this product's known build/backup locations, never other apps.
shopt -s nullglob
for old in "$build_dir/bin/$app_name.app" \
           "$build_dir/app-backups/$app_name"*.app \
           "$HOME/.Trash/$app_name"*.app \
           "$HOME/.Trash/$app_name.app "* \
           "/Applications/.$app_name.app.old"* \
           "/Applications/.$app_name.app.staging"*; do
  [ -d "$old" ] || continue
  old_id=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$old/Contents/Info.plist" 2>/dev/null || true)
  if [ "$old_id" = "$bundle_id" ] || [ "$old" = "$build_dir/bin/$app_name.app" ]; then
    "$lsregister" -u "$old" >/dev/null 2>&1 || true
    remove_app "$old"
    printf 'Removed duplicate app: %s\n' "$old"
  fi
done
for old in "$staging_parent"/stage.*/"$app_name.app"; do
  remove_app "$old"
  rmdir "$(dirname "$old")"
done
rmdir "$staging_parent" 2>/dev/null || true
"$lsregister" -u "$built" >/dev/null 2>&1 || true
"$lsregister" -f "$installed"
"$installed/Contents/MacOS/$exec_name" --version
printf 'Installed: %s (no app backup). Restart the app to use this build.\n' "$installed"
