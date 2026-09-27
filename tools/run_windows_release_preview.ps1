$ErrorActionPreference = 'Stop'

& (Join-Path $PSScriptRoot 'run_windows_preview.ps1') -Configuration Release
