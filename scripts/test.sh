#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
[ -d build ] || ./scripts/build.sh
ctest --test-dir build --output-on-failure
./build/astra-sample --headless-smoke
