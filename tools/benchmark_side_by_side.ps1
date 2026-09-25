param(
    [Parameter(Mandatory = $true)]
    [string]$LegacyExecutable,

    [string]$NewExecutable = '',

    [string]$InputImage = '',

    [ValidateRange(1, 10)]
    [int]$WarmRuns = 3,

    [string]$OutputPath = ''
)

$ErrorActionPreference = 'Stop'
$expectedLegacyHash = '7A97A25CF58ADCF55E81D714A62A51D1F65FF2298BD3EA6B05348415F17F0C9A'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($NewExecutable)) {
    $NewExecutable = Join-Path $repositoryRoot `
        'build\windows-mingw-release\bin\newconvert9918-cli.exe'
}
if ([string]::IsNullOrWhiteSpace($InputImage)) {
    $InputImage = Join-Path $repositoryRoot `
        'tests\golden\source\photo-landscape.png'
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repositoryRoot 'build\performance-comparison.json'
}
$legacyPath = (Resolve-Path -LiteralPath $LegacyExecutable).Path
$newPath = (Resolve-Path -LiteralPath $NewExecutable).Path
$inputPath = (Resolve-Path -LiteralPath $InputImage).Path

$legacyHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $legacyPath).Hash
if ($legacyHash -ne $expectedLegacyHash) {
    throw "Unexpected legacy executable hash: $legacyHash"
}

$modes = @(
    [ordered]@{ index = 0; name = 'Bitmap 9918A'; cli = 'bitmap-9918a' },
    [ordered]@{ index = 1; name = 'Greyscale Bitmap 9918A'; cli = 'greyscale-bitmap-9918a' },
    [ordered]@{ index = 2; name = 'Black-and-White Bitmap 9918A'; cli = 'black-and-white-bitmap-9918a' },
    [ordered]@{ index = 3; name = 'Multicolor 9918'; cli = 'multicolor-9918' },
    [ordered]@{ index = 4; name = 'Dual Multicolor 9918'; cli = 'dual-multicolor-9918' },
    [ordered]@{ index = 5; name = 'Half Multicolor 9918A'; cli = 'half-multicolor-9918a' },
    [ordered]@{ index = 6; name = 'Bitmap Color Only 9918A'; cli = 'bitmap-color-only-9918a' },
    [ordered]@{ index = 7; name = 'Paletted Bitmap F18A'; cli = 'paletted-bitmap-f18a' },
    [ordered]@{ index = 8; name = 'Scanline Palette Bitmap F18A'; cli = 'scanline-palette-bitmap-f18a' }
)

function Invoke-MeasuredProcess {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$WorkingDirectory
    )

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    if ($null -ne $startInfo.ArgumentList) {
        foreach ($argument in $Arguments) {
            $startInfo.ArgumentList.Add($argument)
        }
    } else {
        # Windows PowerShell 5.1 uses .NET Framework, whose ProcessStartInfo
        # predates ArgumentList. These benchmark arguments never contain
        # literal quote characters, so ordinary Windows quoting is sufficient.
        $startInfo.Arguments = ($Arguments | ForEach-Object {
            if ($_ -match '"') {
                throw 'Benchmark process arguments cannot contain quote characters.'
            }
            '"' + $_ + '"'
        }) -join ' '
    }

    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    if (-not $process.Start()) {
        throw "Could not start $Executable"
    }
    [long]$peakWorkingSet = 0
    while (-not $process.WaitForExit(10)) {
        $process.Refresh()
        $peakWorkingSet = [Math]::Max($peakWorkingSet, $process.WorkingSet64)
    }
    $process.WaitForExit()
    $standardOutput = $process.StandardOutput.ReadToEnd()
    $standardError = $process.StandardError.ReadToEnd()
    $process.Refresh()
    $peakWorkingSet = [Math]::Max($peakWorkingSet, $process.PeakWorkingSet64)
    $stopwatch.Stop()

    $record = [ordered]@{
        wall_ms = [Math]::Round($stopwatch.Elapsed.TotalMilliseconds, 3)
        cpu_ms = [Math]::Round($process.TotalProcessorTime.TotalMilliseconds, 3)
        peak_working_set_bytes = $peakWorkingSet
        exit_code = $process.ExitCode
    }
    if ($process.ExitCode -ne 0) {
        throw "$Executable failed with exit code $($process.ExitCode). stdout=$standardOutput stderr=$standardError"
    }
    return $record
}

function Get-Median {
    param([double[]]$Values)
    $ordered = @($Values | Sort-Object)
    $middle = [Math]::Floor($ordered.Count / 2)
    if (($ordered.Count % 2) -eq 1) {
        return $ordered[$middle]
    }
    return ($ordered[$middle - 1] + $ordered[$middle]) / 2.0
}

$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) `
    ('newconvert9918-performance-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporaryRoot | Out-Null

try {
    $modeResults = @()
    foreach ($mode in $modes) {
        Write-Output "Benchmarking $($mode.name)..."
        $legacyRuns = @()
        $newRuns = @()
        for ($run = 0; $run -le $WarmRuns; ++$run) {
            $legacyDirectory = Join-Path $temporaryRoot "legacy-$($mode.index)-$run"
            $newDirectory = Join-Path $temporaryRoot "new-$($mode.index)-$run"
            New-Item -ItemType Directory -Path $legacyDirectory | Out-Null
            New-Item -ItemType Directory -Path $newDirectory | Out-Null

            $legacyArguments = @(
                $inputPath,
                (Join-Path $legacyDirectory 'benchmark'),
                "/CtrlList=$($mode.index)"
            )
            $newArguments = @(
                '--input', $inputPath,
                '--output', $newDirectory,
                '--mode', $mode.cli,
                '--preset', 'balanced',
                '--format', 'tifiles',
                '--json'
            )

            if (($run % 2) -eq 0) {
                $legacyRuns += Invoke-MeasuredProcess $legacyPath $legacyArguments $legacyDirectory
                $newRuns += Invoke-MeasuredProcess $newPath $newArguments $newDirectory
            } else {
                $newRuns += Invoke-MeasuredProcess $newPath $newArguments $newDirectory
                $legacyRuns += Invoke-MeasuredProcess $legacyPath $legacyArguments $legacyDirectory
            }
        }

        $legacyWarmWall = [double[]]@($legacyRuns[1..$WarmRuns] | ForEach-Object { $_.wall_ms })
        $newWarmWall = [double[]]@($newRuns[1..$WarmRuns] | ForEach-Object { $_.wall_ms })
        $legacyMedian = Get-Median $legacyWarmWall
        $newMedian = Get-Median $newWarmWall
        $modeResults += [ordered]@{
            mode_index = $mode.index
            mode = $mode.name
            legacy = [ordered]@{
                cold = $legacyRuns[0]
                warm_runs = @($legacyRuns[1..$WarmRuns])
                warm_median_wall_ms = [Math]::Round($legacyMedian, 3)
                warm_median_cpu_ms = [Math]::Round((Get-Median ([double[]]@(
                    $legacyRuns[1..$WarmRuns] | ForEach-Object { $_.cpu_ms }))), 3)
            }
            new = [ordered]@{
                cold = $newRuns[0]
                warm_runs = @($newRuns[1..$WarmRuns])
                warm_median_wall_ms = [Math]::Round($newMedian, 3)
                warm_median_cpu_ms = [Math]::Round((Get-Median ([double[]]@(
                    $newRuns[1..$WarmRuns] | ForEach-Object { $_.cpu_ms }))), 3)
            }
            new_to_legacy_wall_ratio = if ($legacyMedian -gt 0) {
                [Math]::Round($newMedian / $legacyMedian, 2)
            } else { $null }
        }
    }

    $result = [ordered]@{
        schema_version = 1
        measured_at = [DateTimeOffset]::Now.ToString('o')
        host = [ordered]@{
            operating_system = [Environment]::OSVersion.VersionString
            logical_processors = [Environment]::ProcessorCount
        }
        methodology = [ordered]@{
            input = $inputPath.Substring($repositoryRoot.Length + 1).Replace('\', '/')
            input_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $inputPath).Hash.ToLowerInvariant()
            cold_runs_per_mode = 1
            warm_runs_per_mode = $WarmRuns
            settings = 'Audited defaults / balanced preset'
            export = 'TIFILES tables; legacy also writes its command-line BMP preview where supported'
            measurement_scope = 'Fresh process wall time, total processor time, and peak working set'
        }
        executables = [ordered]@{
            legacy = [ordered]@{
                path = $legacyPath
                sha256 = $legacyHash.ToLowerInvariant()
                version = '1.9.1.0'
            }
            new = [ordered]@{
                path = $newPath
                sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $newPath).Hash.ToLowerInvariant()
                build = 'Release'
            }
        }
        modes = $modeResults
    }

    $resolvedOutput = [IO.Path]::GetFullPath($OutputPath)
    $outputDirectory = Split-Path -Parent $resolvedOutput
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
    [IO.File]::WriteAllText(
        $resolvedOutput,
        ($result | ConvertTo-Json -Depth 10) + "`n",
        [Text.UTF8Encoding]::new($false))
    Write-Output $resolvedOutput
} finally {
    Remove-Item -LiteralPath $temporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
}
