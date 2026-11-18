#!/usr/bin/env bash
set -euo pipefail
for tool in cmake g++; do
  command -v "$tool" >/dev/null 2>&1 || { echo "Error: $tool is required." >&2; exit 1; }
done
cd "$(dirname "$0")/.."
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
echo "Built astra-editor, astra-sample, astra-build, and astra_tests in ./build/"
