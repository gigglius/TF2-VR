#!/usr/bin/env bash
set -euo pipefail

SDK_DIR="third_party/source-sdk-2013"
EXPECTED_REVISION="e1f1fd42271805072f052051c26c21caa2c87b04"

if [ ! -e "$SDK_DIR/.git" ]; then
  echo "ERROR: Valve Source SDK submodule is missing. Check out with submodules enabled."
  exit 1
fi

ACTUAL_REVISION="$(git -C "$SDK_DIR" rev-parse HEAD)"
if [ "$ACTUAL_REVISION" != "$EXPECTED_REVISION" ]; then
  echo "ERROR: Source SDK revision mismatch."
  echo "Expected: $EXPECTED_REVISION"
  echo "Actual:   $ACTUAL_REVISION"
  exit 1
fi

for project in src/game/client/client_tf.vpc src/game/server/server_tf.vpc; do
  if [ ! -s "$SDK_DIR/$project" ]; then
    echo "ERROR: Required TF2 project file is missing: $project"
    exit 1
  fi
done

count_sources() {
  find "$1" -type f | grep -E '\.(cpp|h)$' | wc -l | tr -d ' '
}

ENGINE_SOURCE_COUNT="$(find "$SDK_DIR/src/engine" -type f 2>/dev/null | wc -l | tr -d ' ')"
TF_CLIENT_SOURCE_COUNT="$(count_sources "$SDK_DIR/src/game/client")"
TF_SHARED_SOURCE_COUNT="$(count_sources "$SDK_DIR/src/game/shared/tf")"

echo "Valve Source SDK source audit passed."
echo "Pinned revision: $ACTUAL_REVISION"
echo "TF2 client project: $SDK_DIR/src/game/client/client_tf.vpc"
echo "TF2 server project: $SDK_DIR/src/game/server/server_tf.vpc"
echo "Client source/header files: $TF_CLIENT_SOURCE_COUNT"
echo "TF shared source/header files: $TF_SHARED_SOURCE_COUNT"
echo "Files under src/engine: $ENGINE_SOURCE_COUNT"
echo "NOTE: This verifies source availability only; it does not claim an Android engine build."
