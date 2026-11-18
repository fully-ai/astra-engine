$ErrorActionPreference = "Stop"
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Write-Error "CMake 3.20+ required."; exit 1 }
Set-Location (Join-Path $PSScriptRoot "..")
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
