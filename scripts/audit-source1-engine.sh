#!/usr/bin/env bash
set -euo pipefail

SDK_DIR="third_party/source-sdk-2013"
EXPECTED_REVISION="e1f1fd42271805072f052051c26c21caa2c87b04"

if [ ! -d "$SDK_DIR/.git" ] && [ ! -f "$SDK_DIR/.git" ]; then
  echo "ERROR: Valve Source SDK submodule is missing."
  exit 1
fi

ACTUAL_REVISION="$(git -C "$SDK_DIR" rev-parse HEAD)"
if [ "$ACTUAL_REVISION" != "$EXPECTED_REVISION" ]; then
  echo "ERROR: Source SDK revision mismatch: $ACTUAL_REVISION"
  exit 1
fi

ENGINE_CPP_COUNT="$(find "$SDK_DIR/src/engine" -type f -name '*.cpp' | wc -l | tr -d ' ')"
ENGINE_HEADER_COUNT="$(find "$SDK_DIR/src/engine" -type f \( -name '*.h' -o -name '*.hpp' \) | wc -l | tr -d ' ')"
ENGINE_FILE_COUNT="$(find "$SDK_DIR/src/engine" -type f | wc -l | tr -d ' ')"

echo "Source 1 engine availability audit"
echo "Pinned Valve revision: $ACTUAL_REVISION"
echo "Files under src/engine: $ENGINE_FILE_COUNT"
echo "Engine implementation .cpp files: $ENGINE_CPP_COUNT"
echo "Engine headers: $ENGINE_HEADER_COUNT"

if [ "$ENGINE_CPP_COUNT" -eq 0 ]; then
  echo "::warning::The pinned public SDK contains no Source engine .cpp implementation files."
  echo "The SDK cannot currently produce a standalone Source engine runtime."
fi

if [ "$ENGINE_FILE_COUNT" -eq 1 ] && [ ! -s "$SDK_DIR/src/engine/audio/public/sound.h" ]; then
  echo "ERROR: Unexpected engine directory contents."
  exit 1
fi

echo "Audit complete. This is a source-availability check, not an engine build."
