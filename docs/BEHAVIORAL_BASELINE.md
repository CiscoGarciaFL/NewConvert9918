# Original Convert9918 behavioral baseline

This document records the behavior that New Convert 9918 must be able to test
and intentionally reproduce or supersede. It is an inventory, not permission
to copy implementation code from the original application.

## Reference identity and isolation

- Upstream repository: <https://github.com/tursilion/convert9918>
- Audited commit: [`edbdf0f978d93ad4493e0a91ac1380752e8a082c`](https://github.com/tursilion/convert9918/commit/edbdf0f978d93ad4493e0a91ac1380752e8a082c)
- Reported application version: 1.9.1 / 191
- Local reference checkout: `C:\Users\Cisco\projects\Convert9918-reference`
- Reference status when captured: clean `main` tracking `origin/main`

The reference checkout is deliberately outside this repository. It contains
the original Windows/MFC project, distribution archive, and ImgSource-related
material. None of those files may be copied, linked, vendored, or packaged in
New Convert 9918. Only observed behavior, independently written test fixtures,
and factual format descriptions may cross the boundary.

If upstream changes, keep this commit as the compatibility baseline until a
new revision is explicitly reviewed and recorded here.

## User workflow

The original application is a single Windows dialog with two enlarged image
views and configuration controls around them. Its main workflow is:

1. Open or drop a source image.
2. Choose a conversion mode and settings.
3. Press **Reload** to recompute most setting changes.
4. Inspect the converted image, optionally with F18A palette visualization.
5. Save one or more generated files using **Save Pic**.

Changing a mode without reloading can leave the prior conversion active. The
application warns when saving in that state. New Convert 9918 will preserve the
result semantics but replace this stale-state workflow with automatic,
cancellable preview generation.

The original also has two secondary workflows:

- One input command-line argument opens that file in the GUI. A second output
  argument runs without the GUI and writes TIFILES pattern/color output plus a
  BMP preview.
- Double-clicking unused dialog space enters a random-folder slideshow mode.
  That mode recursively indexes supported images, watches the Windows
  clipboard, and shares the most recently loaded filename with other running
  instances. This is historical behavior, not a v1 parity requirement unless
  separately approved.

## Conversion modes

| # | Original UI name | Principal output tables |
| --- | --- | --- |
| 0 | Bitmap 9918A | 6 KiB pattern + 6 KiB color |
| 1 | Greyscale Bitmap 9918A | 6 KiB pattern + 6 KiB color using a greyscale view of the palette |
| 2 | B&W Bitmap 9918A | 6 KiB pattern; fixed black/white color setup, so no color table |
| 3 | Multicolor 9918 | 1.5 KiB multicolor pattern data |
| 4 | Dual-Multicolor (flicker) 9918 | Two 1.5 KiB multicolor frames |
| 5 | Half-Multicolor (flicker) 9918A | 6 KiB pattern + 6 KiB color + 2 KiB multicolor data |
| 6 | Bitmap color only 9918A | 6 KiB fixed pattern + 6 KiB color |
| 7 | Paletted Bitmap F18A | 6 KiB pattern + 6 KiB color + 32-byte palette |
| 8 | Scanline Palette Bitmap F18A | 6 KiB pattern + 6 KiB color + 6 KiB scanline palettes |

The numeric order matters because the original stores the selected index and
uses it to enable controls and determine table sizes. The portable API uses
named enum values, but golden-test metadata should retain the original index.

## Controls and options

### Source positioning and scaling

| Control | Values or behavior |
| --- | --- |
| Nudge Right / Left | One horizontal source pixel per click, capped at seven pixels in either direction |
| Nudge Up / Down | One pixel; Shift changes the step to 2, Ctrl to 4, and Shift+Ctrl to 8 |
| Scaling filter | Box, Gaussian, Hamming, Blackman, Bilinear (default), None |
| Fill mode | Default/full image, Top/Left crop, Middle crop, Bottom/Right crop |
| PowerPaint | Scale to the Adam PowerPaint 240x160 active area and pad to 256x192 |

Loading a new source clears the nudge offsets. Vertical nudging is meaningful
when cropping a source that is larger than the retained target view.

### Color adjustment

| Control | Range/default | Behavior |
| --- | --- | --- |
| Hist | Off | Histogram stretch before color reduction |
| Perc | Off | Perceptually weighted RGB matching instead of the default YCrCb-style distance |
| Toon | Off | Restricted/cartoon color matching intended for large flat-color regions |
| Perceptual R/G/B | 30/52/18 percent | Channel weights; **Default** restores these values |
| Luma emphasis | 120 percent | Relative importance of luminance in non-perceptual matching |
| Gamma correct | 100 percent | Gamma 1.0; larger values brighten dark areas |
| Max color shift | 1 percent, range 0-100 | Moves source colors toward the nearest target color before dithering |
| Flicker max difference | 95 percent, range 1-100 | Limits luminance difference between flickering colors |
| Static color count | 0, range 0-14 | Fixed colors shared by F18A scanline palettes |
| Static color selection | Median Cut or Popularity | Algorithm used to choose fixed F18A palette colors |
| Region 1/2/3 | On | Include each vertical bitmap third when selecting static palette colors |
| Show Palette | Off | Draw the active F18A palette beside the converted image after Reload |

The palette itself is configurable and persisted in `convert9918.ini`. Golden
fixtures must therefore record the complete palette, not just the visible
settings above.

### Dithering

The original exposes six error-distribution cells, each constrained to 0-16,
plus Average/Accumulate error handling. Presets are:

| Preset | Six distribution values | Ordered behavior |
| --- | --- | --- |
| Floyd-Steinberg | 3, 5, 1, 7, 0, 0 | Off |
| Atkinson | 2, 2, 2, 2, 1, 1 | Off |
| Pattern | 0, 8, 0, 8, 0, 0 | Off |
| Diag | 1, 3, 2, 3, 1, 1 | Off |
| None | 0, 0, 0, 0, 0, 0 | Off |
| Order1 | Cells disabled | 2x2 ordered dither |
| Order2 | 1, 2, 2, 2, 0, 0 | 2x2 ordered dither plus error distribution |
| Order3 | Cells disabled | 4x4 ordered dither |
| Order4 | 1, 2, 2, 2, 0, 0 | 4x4 ordered dither plus error distribution |

An ordered-dither brightness slider ranges from 0 to 16. Distribution totals
below 16 intentionally lose error; totals above 16 accumulate it and may
produce artifacts. The original initializes the six cells to
`2, 2, 2, 2, 1, 1`, and initializes error handling to Accumulate even though
older prose describes Average as the default. Executable observations should
resolve that documentation discrepancy before golden fixtures are approved.

## Input formats

### Raster image readers

- BMP
- GIF (independent in-tree loader; first decoded image behavior must be
  measured, including transparency and animation)
- JPEG (`.jpg` and `.jpeg`)
- PNG
- PCX
- TIFF (`.tif` and `.tiff`)

The original delegates BMP, JPEG, PNG, PCX, TIFF, scaling, histogram work, and
drawing to ImgSource. New Convert 9918 must use Qt image I/O or independently
implemented codecs and must define color-space, orientation, alpha, animation,
allocation-limit, and malformed-input policies explicitly.

### Retro readers

- TI Artist pattern/color/multicolor sets: `.TIAP`, `.TIAC`, `.TIAM`, `_P`,
  `_C`, and `_M`; recognizes TIFILES and V9T9 headers, with modifier-key paths
  for raw or RLE data
- MSX Screen 2: `.SC2`
- Coleco CVPaint: `.PC`
- Adam PowerPaint: `.PP`
- Adam HGR: `.HGR` and `.HGRH`

The random-folder mode also lists `.jpc`, but the audited load switch contains
no matching decoder. Treat that as a suspected original defect, not supported
input, until confirmed against the executable.

### Interactive input

- Windows file-open dialog
- File drag and drop
- Windows clipboard bitmap polling in slideshow mode
- Shared-memory filename notification between original application instances

## Output formats

| Save choice | Files/headers | Known applicability |
| --- | --- | --- |
| TIFILES | Pattern/color/multicolor files with 128-byte TIFILES headers | Table-producing modes |
| V9T9 | Pattern/color/multicolor files with 128-byte V9T9 headers and six-character base names | Table-producing modes |
| Raw | Headerless pattern/color/multicolor files | Table-producing modes |
| RLE | Headerless RLE pattern/color data; F18A palette data remains uncompressed | Table-producing modes |
| TI XB Program | TIFILES Extended BASIC display program | Bitmap 9918A family only; rejects multicolor and F18A palette modes |
| TI XB RLE Program | RLE Extended BASIC display program | Same mode restrictions as XB Program |
| MSX SC2 | `.SC2` VRAM-style image | Bitmap 9918A family only |
| Coleco CVPaint | `.PC`, pattern then color data | Format limitations require executable fixtures per mode |
| Adam PowerPaint 10k | `.PP`, reduced 240x160 area | Does not support multicolor-table output |
| Adam HGR | `.HGR`/`.HGRH`, fixed header, color then pattern data | Does not support multicolor-table output |
| ColecoVision Cart | `.ROM` using the embedded ROM template | Bitmap 9918A family only |
| ColecoVision RLE Cart | `.ROM` | UI explicitly labels this option broken |
| PNG file (PC) | Paletted `.PNG` preview/export | Scanline-palette mode rejected |

The table names traditionally use `.TIAP`, `.TIAC`, and `.TIAM`; RAW/RLE and
V9T9 naming varies by save choice. Each golden case must store the exact list
of generated filenames, byte sizes, headers, and ordering as well as content.

## Persistent and hidden state

The original reads command-line overrides and `convert9918.ini` from the
current working directory. Persisted state includes:

- Six error-distribution values
- Ordered mode, map size, and ordered brightness
- Conversion-mode index, scaling filter, fill mode, and PowerPaint flag
- Perceptual and cartoon flags, RGB weights, luma emphasis, and gamma
- Error accumulation, histogram stretch, max color shift, and flicker limit
- Horizontal/vertical offsets and scale mode
- Working and default 15-color palette entries

Additional process-global state owns source dimensions, crop rectangles,
source and converted buffers, palette buffers, scanline palettes, generated
table sizes, random-folder lists, command-line pointers, current dialog
pointer, conversion validity, and shared-memory synchronization. Conversion
functions read and mutate these globals directly rather than receiving a
complete request and returning an owned result.

## Windows and MFC coupling

The audited source is a Visual Studio `v143` MFC Windows-subsystem project and
is coupled to Windows in the following areas:

- `CWinApp`, `CDialog`, `CWnd`, message maps, dialog resources, MFC data
  exchange/validation, `CString`, `CDC`, and `AfxMessageBox`
- `HWND`, `HANDLE`, `HGLOBAL`, `BYTE`, `RGBQUAD`, `GlobalAlloc`, and
  `GlobalFree` in conversion and image-buffer paths
- Win32 file dialogs, drag/drop, keyboard modifier polling, clipboard APIs,
  timers, recursive file enumeration, and INI profile APIs
- Named shared memory plus `Interlocked*` operations for cross-instance image
  notification
- Direct drawing through a window device context from conversion/quantization
  code
- Dynamic `user32.dll` loading for DPI handling
- Absolute developer-machine ImgSource include and library paths in source and
  project files, including references to the obsolete `ISLibMS40.lib` static
  library

The distributed 1.9.1 executable runs without a separate ImgSource DLL, which
is consistent with that dependency having been linked into the original
binary. The clean implementation uses that executable only as a behavioral
oracle: it does not need to rebuild, extract, redistribute, or link the old
library or its object code.

Portable replacements must keep all of these concerns at the application or
Qt adapter boundary. Core conversion calls must operate on explicit standard
C++ request/result values with owned or clearly viewed buffers, deterministic
settings, diagnostics, and no drawing or dialog side effects.

## Original defects and compatibility questions

Confirmed defects and their intentional-compatibility policy are tracked in
[`ORIGINAL_DEFECTS.md`](ORIGINAL_DEFECTS.md). The remaining questions need
executable tests before deciding whether to preserve or fix them:

- The save code itself questions whether RLE TIFILES headers contain the wrong
  file size.
- `.jpc` files are indexed by slideshow mode but have no matching reader in
  the audited dispatch.
- Documentation and initialized state disagree about Average versus Accumulate
  as the default error mode.
- Documentation describes older 30/59/11 perceptual weights, while version
  191 initializes and resets to 30/52/18.
- GIF comments acknowledge incomplete animated-transparency behavior.
- TI Artist raw/RLE interpretation depends on Shift/Ctrl/Alt state at load
  time, which is not self-describing or suitable for a portable CLI.
- Per-scanline F18A PNG export is rejected.
- Save validity depends on a manual Reload and can refer to the prior mode.

Compatibility fixtures should preserve proven byte-level output even when the
UI or API becomes safer. Confirmed defects are documented and tested as
intentional differences rather than copied accidentally.

## Golden-corpus metadata

Every captured case should include:

- Source asset identifier, license/provenance, SHA-256, dimensions, pixel
  format, color profile, alpha/orientation information, and malformed status
- Original executable version and reference commit
- Conversion-mode name and original numeric index
- Full palette plus every setting and hidden modifier used for input
- Preview image and every generated file with filename, size, and SHA-256
- Original stdout/warnings and whether Reload occurred before saving
- Expected compatibility class: byte-exact, image-exact, visually reviewed,
  intentional improvement, or known original defect
- Reviewer and approval date for public distribution of the fixture

The next Phase 2 task is to select redistributable source assets covering the
roadmap categories, then execute this capture protocol on Windows.
