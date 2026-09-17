$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $projectRoot 'build/windows-mingw-debug/bin/NewConvert9918.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw 'Build the windows-mingw-debug preset before launching the preview.'
}

$runtime = Join-Path (Split-Path -Parent $executable) 'Qt6Core.dll'
if (-not (Test-Path -LiteralPath $runtime -PathType Leaf)) {
    & (Join-Path $PSScriptRoot 'deploy_windows_preview.ps1') -SkipBuild
}

& $executable
