#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_config=$(mktemp -d "${TMPDIR:-/tmp}/aseprite-plus-navigation.XXXXXX")
trap 'result=$?; if [ "$result" -eq 0 ]; then rm -rf "$test_config"; else printf "Test settings retained: %s\n" "$test_config"; fi' EXIT
ASEPRITE_USER_FOLDER="$test_config" "$project_dir/build/bin/oldking_navigation_tests"
ASEPRITE_USER_FOLDER="$test_config" "$project_dir/build/bin/oldking_navigation_tests" --verify-saved
