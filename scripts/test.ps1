$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
ctest --test-dir build --output-on-failure -C Release
