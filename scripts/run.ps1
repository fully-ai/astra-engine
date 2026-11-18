$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
& ".\build\Release\astra-editor.exe"
