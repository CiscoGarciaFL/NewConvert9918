param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$preset = "windows-mingw-$($Configuration.ToLowerInvariant())"
$executable = Join-Path $projectRoot "build/$preset/bin/NewConvert9918.exe"
$deployArguments = @{ Configuration = $Configuration }
if ($SkipBuild) { $deployArguments.SkipBuild = $true }
& (Join-Path $PSScriptRoot 'deploy_windows_preview.ps1') @deployArguments

& $executable
