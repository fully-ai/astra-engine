#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
[ -x build/astra-editor ] || ./scripts/build.sh
exec ./build/astra-editor "$@"
