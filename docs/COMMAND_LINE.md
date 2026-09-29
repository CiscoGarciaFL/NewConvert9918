# Command-line interface

`retrovdp-cli` is the headless automation frontend for RetroVDP Studio.
It uses the same bounded image loaders, transform and adjustment pipeline,
conversion core, export manifest generators, and atomic file writer as the
desktop application. It does not open a window or read the desktop
application's persisted settings.

It can instead load the versioned recipe saved by the desktop interface. The
recipe supplies the source path and every Screen Image conversion-panel value;
the script supplies the output directory:

```shell
retrovdp-cli \
  --recipe artwork.rvdp.json \
  --output converted
```

`--input`, `--target`, `--mode`, `--preset`, and `--format` override their
recipe values when explicitly supplied. Character and Sprite recipes retain
their editor and set state but are rejected by the CLI until those
hardware-data converters are implemented.

## One-shot conversion

Both the source file and destination directory are required:

```shell
retrovdp-cli \
  --input artwork.png \
  --output converted \
  --target tms9918a \
  --mode bitmap-9918a \
  --preset balanced \
  --format tifiles
```

The output directory is created when necessary. Existing files are rejected
as a complete preflight operation; no file is changed unless `--overwrite` is
explicitly supplied.

Use `retrovdp-cli --help` for the installed executable's complete option
summary.

## Targets, modes, presets, and formats

The implemented target names are `tms9918a` and `f18a`. The default target is
`tms9918a`; the default mode is `bitmap-9918a`, the default preset is
`balanced`, and the default format is `tifiles`. A target accepts only modes
declared compatible by its profile. V9938 is registered as planned but is not
offered by the CLI until conversion support is implemented.

Supported modes are:

- `bitmap-9918a`
- `greyscale-bitmap-9918a`
- `black-and-white-bitmap-9918a`
- `multicolor-9918`
- `dual-multicolor-9918`
- `half-multicolor-9918a`
- `bitmap-color-only-9918a`
- `paletted-bitmap-f18a`
- `scanline-palette-bitmap-f18a`

Supported presets are `balanced`, `crisp-pixel-art`, `smooth-photograph`, and
`ordered-retro`. The shorter aliases `crisp`, `smooth`, and `ordered` are also
accepted. A preset establishes deterministic conversion settings and scaling;
`--mode` then selects the target hardware layout.

Supported export formats are `tifiles`, `v9t9`, `raw`, `rle`, `msx-sc2`,
`coleco-cvpaint`, `adam-powerpaint`, `adam-hgr`, and `png`. Applicability is
validated against the selected conversion mode before any output is written.

`--base-name` overrides the source-derived output name. Characters outside
letters, digits, underscore, and hyphen are replaced with underscores.

## Automation and JSON

Pass `--json` to receive exactly one compact JSON object on standard output.
A successful object includes the resolved input/output paths, stable target
identifier, selected mode, preset and format, generated file paths and byte
counts, and any manifest warnings:

```json
{"status":"ok","target":"tms9918a","mode":"bitmap-9918a","preset":"balanced","format":"raw","files":[{"path":"/output/ART.TIAP","bytes":6144}],"warnings":[]}
```

Failures contain `status: "error"`, a stable diagnostic `code`, a human-readable
`message`, and the numeric `exitCode`. Output conflicts additionally list all
conflicting paths under `details.conflicts`.

## Exit codes

| Code | Meaning |
| ---: | --- |
| `0` | Conversion and export completed successfully. |
| `2` | Command syntax, required option, target, mode, preset, format, or base name is invalid. |
| `3` | The source image could not be loaded safely. |
| `4` | Image transformation or conversion failed. |
| `5` | The selected export is unavailable or could not be generated. |
| `6` | Output conflicts were found or files could not be written. |

The CLI never prompts. Automation must use `--overwrite` deliberately when
replacement is intended and should treat every nonzero code as failure.

## Verification

`cli_workflow_validation` launches the built CLI as a separate process. It
checks help output, explicit target selection, a complete image-to-RAW
conversion, JSON results and file sizes, overwrite preflight, explicit
overwrite, invalid-mode diagnostics, and input-load failure codes.
