$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $projectRoot 'build/windows-mingw-debug/bin/NewConvert9918.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw 'Build the windows-mingw-debug preset before launching the preview.'
}

$qtRuntime = 'C:\Qt\6.10.3\mingw_64\bin'
$compilerRuntime = 'C:\Qt\Tools\mingw1310_64\bin'
$env:Path = "$qtRuntime;$compilerRuntime;$env:Path"

& $executable
