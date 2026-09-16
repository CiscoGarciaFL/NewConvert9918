param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = 'Stop'
$expectedExecutableHash = '7A97A25CF58ADCF55E81D714A62A51D1F65FF2298BD3EA6B05348415F17F0C9A'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$corpusRoot = Join-Path $repositoryRoot 'tests\golden'
$versionRoot = Join-Path $corpusRoot 'reference\original-1_9_1'
$manifestPath = Join-Path $corpusRoot 'corpus.json'
$baselineManifestPath = Join-Path $versionRoot 'capture.json'

$modeDefinitions = @(
    [ordered]@{
        index = 1
        name = 'Greyscale Bitmap 9918A'
        directory = 'greyscale-bitmap-9918a'
        preview_available = $true
        tables = @(
            [ordered]@{ extension = 'TIAP'; role = 'pattern'; payload_size = 6144 },
            [ordered]@{ extension = 'TIAC'; role = 'color'; payload_size = 6144 }
        )
    },
    [ordered]@{
        index = 2
        name = 'B&W Bitmap 9918A'
        directory = 'black-white-bitmap-9918a'
        preview_available = $true
        tables = @(
            [ordered]@{ extension = 'TIAP'; role = 'pattern'; payload_size = 6144 }
        )
    },
    [ordered]@{
        index = 3
        name = 'Multicolor 9918'
        directory = 'multicolor-9918'
        preview_available = $true
        tables = @(
            [ordered]@{ extension = 'TIAP'; role = 'multicolor'; payload_size = 1536 }
        )
    },
    [ordered]@{
        index = 4
        name = 'Dual-Multicolor (flicker) 9918'
        directory = 'dual-multicolor-9918'
        preview_available = $true
        tables = @(
            [ordered]@{ extension = 'TIAP'; role = 'multicolor-frame-1'; payload_size = 1536 },
            [ordered]@{ extension = 'TIAC'; role = 'multicolor-frame-2'; payload_size = 1536 }
        )
    },
    [ordered]@{
        index = 5
        name = 'Half-Multicolor (flicker) 9918A'
        directory = 'half-multicolor-9918a'
        preview_available = $true
        tables = @(
            [ordered]@{ extension = 'TIAP'; role = 'pattern'; payload_size = 6144 },
            [ordered]@{ extension = 'TIAC'; role = 'color'; payload_size = 6144 },
            [ordered]@{ extension = 'TIAM'; role = 'multicolor'; payload_size = 2048 }
        )
    },
    [ordered]@{
        index = 6
        name = 'Bitmap color only 9918A'
        directory = 'bitmap-color-only-9918a'
        preview_available = $true
        tables = @(
            [ordered]@{ extension = 'TIAP'; role = 'fixed-pattern'; payload_size = 6144 },
            [ordered]@{ extension = 'TIAC'; role = 'color'; payload_size = 6144 }
        )
    },
    [ordered]@{
        index = 7
        name = 'Paletted Bitmap F18A'
        directory = 'paletted-bitmap-f18a'
        preview_available = $true
        tables = @(
            [ordered]@{ extension = 'TIAP'; role = 'pattern'; payload_size = 6144 },
            [ordered]@{ extension = 'TIAC'; role = 'color'; payload_size = 6144 },
            [ordered]@{ extension = 'TIAM'; role = 'palette'; payload_size = 32 }
        )
    },
    [ordered]@{
        index = 8
        name = 'Scanline Palette Bitmap F18A'
        directory = 'scanline-palette-bitmap-f18a'
        preview_available = $false
        preview_note = 'The original command-line path suppresses BMP output for per-scanline palettes.'
        tables = @(
            [ordered]@{ extension = 'TIAP'; role = 'pattern'; payload_size = 6144 },
            [ordered]@{ extension = 'TIAC'; role = 'color'; payload_size = 6144 },
            [ordered]@{ extension = 'TIAM'; role = 'scanline-palettes'; payload_size = 6144 }
        )
    }
)

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$actualExecutableHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedExecutable).Hash
if ($actualExecutableHash -ne $expectedExecutableHash) {
    throw "Unexpected original executable hash: $actualExecutableHash"
}

$corpus = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$baselineCapture = Get-Content -Raw -LiteralPath $baselineManifestPath | ConvertFrom-Json

# Fail before doing work if any destination already contains files. This keeps
# an approved capture immutable and makes accidental partial overwrites obvious.
foreach ($mode in $modeDefinitions) {
    foreach ($source in $corpus.source_images) {
        $caseName = [IO.Path]::GetFileNameWithoutExtension($source.path)
        $caseDirectory = Join-Path $versionRoot "$($mode.directory)\default\$caseName"
        if ((Test-Path -LiteralPath $caseDirectory) -and
            (Get-ChildItem -LiteralPath $caseDirectory -File -ErrorAction SilentlyContinue)) {
            throw "Capture directory is not empty: $caseDirectory"
        }
    }
}

$modeRecords = @()
foreach ($mode in $modeDefinitions) {
    $captureRecords = @()
    foreach ($source in $corpus.source_images) {
        $inputPath = Join-Path $repositoryRoot ($source.path -replace '/', '\')
        $caseName = [IO.Path]::GetFileNameWithoutExtension($inputPath)
        $caseDirectory = Join-Path $versionRoot "$($mode.directory)\default\$caseName"
        New-Item -ItemType Directory -Force -Path $caseDirectory | Out-Null

        $outputBase = Join-Path $caseDirectory $caseName
        Write-Output "Capturing mode $($mode.index) ($($mode.name)): $caseName"
        $process = Start-Process -FilePath $resolvedExecutable `
            -ArgumentList @($inputPath, $outputBase, "/CtrlList=$($mode.index)") `
            -WorkingDirectory $caseDirectory `
            -WindowStyle Hidden `
            -Wait `
            -PassThru
        if ($process.ExitCode -ne 0) {
            throw "Convert9918 exited with $($process.ExitCode) for mode $($mode.index), $caseName"
        }

        $outputs = @(Get-ChildItem -LiteralPath $caseDirectory -File | Sort-Object Name)
        $expectedExtensions = @($mode.tables | ForEach-Object { $_.extension })
        if ($mode.preview_available) {
            $expectedExtensions += 'BMP'
        }
        $actualExtensions = @($outputs | ForEach-Object { $_.Extension.TrimStart('.').ToUpperInvariant() })
        $extensionDifference = @(Compare-Object $expectedExtensions $actualExtensions)
        if ($extensionDifference.Count -ne 0) {
            throw "Unexpected outputs for mode $($mode.index), $caseName`: $($actualExtensions -join ', ')"
        }

        $outputRecords = foreach ($output in $outputs) {
            $extension = $output.Extension.TrimStart('.').ToUpperInvariant()
            $record = [ordered]@{
                path = $output.FullName.Substring($repositoryRoot.Length + 1).Replace('\', '/')
                kind = if ($extension -eq 'BMP') { 'preview' } else { 'table' }
                extension = $extension
                size = $output.Length
                sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $output.FullName).Hash.ToLowerInvariant()
            }
            if ($extension -ne 'BMP') {
                $table = $mode.tables | Where-Object { $_.extension -eq $extension }
                $record.role = $table.role
                $record.payload_size = $table.payload_size
            }
            $record
        }

        $captureRecords += [ordered]@{
            source = $source.path
            source_sha256 = $source.sha256
            outputs = @($outputRecords)
        }
    }

    $modeRecord = [ordered]@{
        conversion_mode_index = $mode.index
        conversion_mode = $mode.name
        directory = $mode.directory
        preview_available = $mode.preview_available
        table_payloads = @($mode.tables)
        captures = @($captureRecords)
    }
    if ($mode.preview_note) {
        $modeRecord.preview_note = $mode.preview_note
    }
    $modeRecords += $modeRecord
}

$capture = [ordered]@{
    schema_version = 1
    original = $baselineCapture.original
    invocation = [ordered]@{
        syntax = 'convert9918.exe <input> <output-base> /CtrlList=<mode-index>'
        export = 'Applicable TIFILES tables plus command-line BMP preview when supported'
        conversion_mode_range = '1-8; mode 0 remains recorded in capture.json'
    }
    settings = $baselineCapture.settings
    modes = @($modeRecords)
}

$captureManifestPath = Join-Path $versionRoot 'mode-captures.json'
$json = $capture | ConvertTo-Json -Depth 10
[IO.File]::WriteAllText($captureManifestPath, $json + "`n", [Text.UTF8Encoding]::new($false))
Write-Output $captureManifestPath
