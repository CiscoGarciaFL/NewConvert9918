param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$cmake = 'C:\Qt\Tools\CMake_64\bin\cmake.exe'
$deployQt = 'C:\Qt\6.10.3\mingw_64\bin\windeployqt.exe'
$executable = Join-Path $projectRoot 'build/windows-mingw-debug/bin/NewConvert9918.exe'
$qmlDirectory = Join-Path $projectRoot 'app/qml'

foreach ($requiredTool in @($cmake, $deployQt)) {
    if (-not (Test-Path -LiteralPath $requiredTool -PathType Leaf)) {
        throw "Required Qt development tool was not found: $requiredTool"
    }
}

if (-not $SkipBuild) {
    & $cmake --build --preset windows-mingw-debug --target NewConvert9918
    if ($LASTEXITCODE -ne 0) {
        throw "The Windows preview build failed with exit code $LASTEXITCODE."
    }
}

if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw 'NewConvert9918.exe was not built. Configure the windows-mingw-debug preset first.'
}

# The Qt online installer supplies release runtime DLLs for this MinGW kit even
# when our application has Debug symbols, so force the matching release runtime
# names (Qt6Core.dll, qwindows.dll, and friends).
& $deployQt `
    --release `
    --compiler-runtime `
    --qmldir $qmlDirectory `
    $executable
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE."
}

$runtime = Join-Path (Split-Path -Parent $executable) 'Qt6Core.dll'
if (-not (Test-Path -LiteralPath $runtime -PathType Leaf)) {
    throw 'Deployment completed without producing Qt6Core.dll.'
}

Write-Host "Runnable Windows folder: $(Split-Path -Parent $executable)"
