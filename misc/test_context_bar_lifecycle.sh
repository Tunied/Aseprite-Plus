#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_config=$(mktemp -d "${TMPDIR:-/tmp}/aseprite-plus-lifecycle.XXXXXX")
trap 'result=$?; if [ "$result" -eq 0 ]; then rm -rf "$test_config"; else printf "Test settings retained: %s\n" "$test_config"; fi' EXIT
# Repeat in a fresh process with the same saved settings to cover relaunches.
for run in 1 2 3; do
  ASEPRITE_USER_FOLDER="$test_config" "$project_dir/build/bin/oldking_context_bar_lifecycle_tests"
done
