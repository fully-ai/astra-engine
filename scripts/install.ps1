$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
& (Join-Path $PSScriptRoot "build.ps1")
