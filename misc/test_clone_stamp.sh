#!/bin/sh
# Run the built, headless integration suite without touching real user settings.
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_config=$(mktemp -d "${TMPDIR:-/tmp}/aseprite-plus-clone.XXXXXX")
trap 'result=$?; if [ "$result" -eq 0 ]; then rm -rf "$test_config"; else printf "Test settings retained: %s\n" "$test_config"; fi' EXIT
ASEPRITE_USER_FOLDER="$test_config" "$project_dir/build/bin/oldking_clone_tests"
