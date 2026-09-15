param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = 'Stop'
$expectedExecutableHash = '7A97A25CF58ADCF55E81D714A62A51D1F65FF2298BD3EA6B05348415F17F0C9A'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$corpusRoot = Join-Path $repositoryRoot 'tests\golden'
$versionRoot = Join-Path $corpusRoot 'reference\original-1_9_1'
$captureRoot = Join-Path $versionRoot 'bitmap-9918a\default'
$manifestPath = Join-Path $corpusRoot 'corpus.json'

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$actualExecutableHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedExecutable).Hash
if ($actualExecutableHash -ne $expectedExecutableHash) {
    throw "Unexpected original executable hash: $actualExecutableHash"
}

$corpus = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$records = @()

foreach ($source in $corpus.source_images) {
    $inputPath = Join-Path $repositoryRoot ($source.path -replace '/', '\')
    $caseName = [IO.Path]::GetFileNameWithoutExtension($inputPath)
    $caseDirectory = Join-Path $captureRoot $caseName
    New-Item -ItemType Directory -Force -Path $caseDirectory | Out-Null

    if (Get-ChildItem -LiteralPath $caseDirectory -File -ErrorAction SilentlyContinue) {
        throw "Capture directory is not empty: $caseDirectory"
    }

    $outputBase = Join-Path $caseDirectory $caseName
    $process = Start-Process -FilePath $resolvedExecutable `
        -ArgumentList @($inputPath, $outputBase, '/CtrlList=0') `
        -WorkingDirectory $caseDirectory `
        -Wait `
        -PassThru
    if ($process.ExitCode -ne 0) {
        throw "Convert9918 exited with $($process.ExitCode) for $caseName"
    }

    $outputs = Get-ChildItem -LiteralPath $caseDirectory -File | Sort-Object Name
    if ($outputs.Count -ne 3) {
        throw "Expected BMP, TIAC, and TIAP outputs for $caseName; found $($outputs.Count)"
    }

    $outputRecords = foreach ($output in $outputs) {
        [ordered]@{
            path = $output.FullName.Substring($repositoryRoot.Length + 1).Replace('\', '/')
            size = $output.Length
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $output.FullName).Hash.ToLowerInvariant()
        }
    }

    $records += [ordered]@{
        source = $source.path
        source_sha256 = $source.sha256
        outputs = @($outputRecords)
    }
}

$capture = [ordered]@{
    schema_version = 1
    original = [ordered]@{
        application = 'Convert9918'
        version = '1.9.1.0'
        repository_commit = 'edbdf0f978d93ad4493e0a91ac1380752e8a082c'
        executable_sha256 = $expectedExecutableHash.ToLowerInvariant()
    }
    invocation = [ordered]@{
        syntax = 'convert9918.exe <input> <output-base> /CtrlList=0'
        conversion_mode_index = 0
        conversion_mode = 'Bitmap 9918A'
        export = 'TIFILES pattern and color tables plus command-line BMP preview'
    }
    settings = [ordered]@{
        PIXA = 2
        PIXB = 2
        PIXC = 2
        PIXD = 2
        PIXE = 1
        PIXF = 1
        order_slide = 0
        filter = 4
        power_paint = 0
        portrait_mode = 0
        perceptual = 0
        cartoon = 0
        accumulate_errors = 1
        maximum_multicolor_difference = 95
        maximum_color_shift_percent = 1
        ordered_dither = 0
        ordered_map_size = 2
        perceptual_red = 0.30
        perceptual_green = 0.52
        perceptual_blue = 0.18
        luma_emphasis = 1.2
        gamma = 1.0
        stretch_histogram = $false
        pixel_offset = 0
        height_offset = 0
    }
    captures = @($records)
}

$captureManifestPath = Join-Path $versionRoot 'capture.json'
$json = $capture | ConvertTo-Json -Depth 8
[IO.File]::WriteAllText($captureManifestPath, $json + "`n", [Text.UTF8Encoding]::new($false))
Write-Output $captureManifestPath
