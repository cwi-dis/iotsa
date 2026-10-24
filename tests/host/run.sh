#!/bin/sh
# Build and run every tests/host/test_*.cpp on the host (see README.md).
# Exits non-zero if any test fails to build or fails.
set -e
here=$(cd "$(dirname "$0")" && pwd)
src="$here/../../src"
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
status=0
for t in "$here"/test_*.cpp; do
  name=$(basename "$t" .cpp)
  if ! c++ -std=c++17 -Wall -Werror -I"$here" -I"$src" "$t" -o "$out/$name"; then
    echo "$name: build failed"
    status=1
    continue
  fi
  "$out/$name" || status=1
done
exit $status
