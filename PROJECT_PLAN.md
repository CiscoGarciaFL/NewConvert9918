# New Convert 9918 — Project Plan and Checklist

Last updated: 2026-09-27

This document is the durable handoff for future development sessions. Read it
before starting work, update the checkboxes as milestones are completed, and
record important changes in the Decision Log.

## Project identity

- Repository name: `NewConvert9918`
- Application title: **New Convert 9918**
- Local repository: `C:\Users\Cisco\projects\NewConvert9918`
- Intended GitHub owner: `CiscoGarciaFL`
- Visibility: public
- Original project: <https://github.com/tursilion/convert9918>
- Original author: Mike Brent, aka Tursi, of HarmlessLion.com
- New interface and features: Cisco Garcia / CiscoGarciaFL

## Mission

Create a polished, cross-platform successor to Convert9918 while preserving
the original application's excellent conversion output and retro-computer
format support.

The application must run on Windows, Linux, and macOS. The conversion engine
must be testable independently from the interface, and results should match
the original application unless a difference is an intentional, documented
improvement.

## Confirmed decisions

- [x] Use a clean Git history rather than preserving the original repository's
  commits.
- [x] Use C++20 for conversion, codec, and export logic.
- [x] Use Qt 6.8 or newer; develop against the current Qt 6 release.
- [x] Use Qt Quick Controls 2 for the redesigned interface.
- [x] Use CMake as the build system.
- [x] Keep the editor optional; use Zed tasks and CMake presets without making
  project builds depend on an IDE.
- [x] Use Qt MinGW on Windows, GCC or Clang on Linux, and AppleClang on macOS.
- [x] Keep conversion behavior independent of QML and operating-system APIs.
- [x] Replace ImgSource completely rather than redistributing it.
- [x] Use the original Convert9918 license terms with the original author's
  permission.
- [x] Preserve full attribution for Mike Brent/Tursi/HarmlessLion.com.
- [x] Credit Cisco Garcia/CiscoGarciaFL for the cross-platform architecture,
  new interface, user experience, and new features.
- [x] Keep the repository public while restricting direct write and merge
  access to approved maintainers.
- [x] Do not introduce GPL-covered code or dependencies that would change the
  governing license terms.

## Current status

### Completed

- [x] Created the local project directory.
- [x] Initialized a Git repository on `main`.
- [x] Added the Qt Quick application shell.
- [x] Added the initial C++ settings and validation layer.
- [x] Added initial validation tests.
- [x] Added CMake configuration.
- [x] Added architecture and migration notes.
- [x] Added `LICENSE` with the original license text preserved unchanged.
- [x] Added `NOTICE.md` with original and new-work attribution.
- [x] Added controlled-contribution guidance.
- [x] Created and pushed the public GitHub repository at
  <https://github.com/CiscoGarciaFL/NewConvert9918>.
- [x] Installed Qt 6.10.3, MinGW 13.1, CMake 3.30.5, and Ninja 1.12.1.
- [x] Installed Zed 1.19.2 on the Windows development VM.
- [x] Configured and built the application shell with MinGW Makefiles.
- [x] Ran the validation suite successfully (2/2 tests passing).
- [x] Added cross-platform CMake presets and Zed build/test tasks.
- [x] Added shared C++ and QML formatting configuration.
- [x] Added a GitHub Actions build/test matrix for Windows, Ubuntu, Intel macOS,
  and Apple Silicon macOS.
- [x] Protected `main` with pull-request review and cross-platform CI checks.
- [x] Preserved and pinned an isolated original-project reference checkout.
- [x] Inventoried the original controls, modes, formats, global state, and MFC
  coupling in `docs/BEHAVIORAL_BASELINE.md`.
- [x] Added a redistribution-safe golden-test source corpus with generated,
  deterministic, malformed, and cross-dimensional fixtures.
- [x] Added automated corpus digest, decode, dimension, alpha, and rejection
  checks.
- [x] Captured the original 1.9.1 default Bitmap 9918A preview and TIFILES
  tables for all eight valid source fixtures.
- [x] Captured the remaining eight conversion modes with every applicable
  command-line TIFILES table and BMP preview for all eight valid sources.
- [x] Captured all eleven applicable non-TIFILES Save Pic choices through the
  original Windows UI, pinned fourteen output files, and documented the
  explicitly broken ColecoVision RLE cartridge writer as excluded.
- [x] Added the portable `RgbImage` buffer contract with RGB/RGBA formats,
  explicit stride semantics, checked size arithmetic, and allocation limits.
- [x] Defined the shared conversion request/result boundary and structured
  information, warning, and error diagnostics.
- [x] Defined validated 16-color palettes and exact role/size contracts for
  every conversion mode's target-memory tables.
- [x] Added conversion-independent aspect-preserving fit/fill geometry,
  clamped crop positioning, and deterministic RGB/RGBA resampling filters.
- [x] Added legacy-compatible YCrCb and perceptually weighted RGB color
  distances with configurable, validated weights and luma emphasis.
- [x] Added deterministic brightness-histogram stretching and legacy-formula
  gamma correction while preserving alpha and padded row bytes.
- [x] Added deterministic RGB444/RGB888 median cut and legacy-style weighted
  RGB444 popularity palette selection.
- [x] Added the original six-cell error-distribution kernels, bounded error
  buffer, Average/Accumulate behavior, and 2x2/4x4 ordered threshold maps.
- [x] Ported Bitmap 9918A / Graphics II block quantization with the original
  working palette, color-index remapping, preview, and 6 KiB target tables.
- [x] Ported Greyscale Bitmap 9918A with legacy source-luminance and separate
  Rec.709 palette-luminance behavior.
- [x] Ported Black-and-White Bitmap 9918A with a two-color search, normalized
  black pattern bits, and pattern-only target output.
- [x] Ported Bitmap Color Only 9918A with fixed `0xF0` patterns and an ordered
  two-color search for every eight-pixel row.
- [x] Ported Multicolor 9918 with legacy 4x4 logical-pixel matching, hardware
  color remapping, preview expansion, and the 1536-byte table layout.
- [x] Ported Dual Multicolor 9918 with ordered two-frame color mixing, the
  configurable flicker-luminance limit, and separate 1536-byte frame tables.
- [x] Ported Half Multicolor 9918A with a 4x4 underlay, dithered Graphics II
  overlay, legacy flicker constraint, table rotations, and 2 KiB underlay data.
- [x] Ported Paletted Bitmap F18A with an independently selected 15-color
  palette, RGB444 rounding, Graphics II tables, and 32-byte F18A palette data.
- [x] Ported Scanline Palette Bitmap F18A with deterministic per-row RGB444
  palettes, per-row Graphics II matching, preview, and 6 KiB palette data.
- [x] Added cooperative cancellation tokens and a thread-safe preview job
  controller with monotonic generation IDs and stale-result rejection.
- [x] Added a reproducible Release benchmark and conservative buffer-memory
  estimates; the 2026-09-16 VM baseline is recorded in `docs/PERFORMANCE.md`.
- [x] Completed the first performance pass with Release preview tooling,
  in-converter cancellation, prepared color distances, and Multicolor caches.
  Its intermediate side-by-side total was 31.7% faster than the initial New
  Release result and 1.19x the legacy total.
- [x] Completed the legacy compute-parity pass with specialized quantizer
  variants, quantizer-scoped fast floating point, and Release whole-program
  optimization. The final total is 25.872 seconds versus 31.595 seconds for
  legacy (0.82x), with all nine modes at or faster than parity. Full staged
  results are recorded in `docs/PERFORMANCE_COMPARISON.md`.
- [x] Completed a conversion-acceleration research study covering exact
  algorithm reductions, portable runtime SIMD, safe multicore partitioning,
  fixed-point feasibility, and cross-platform GPU options. The benchmark-based
  estimates and staged implementation gates are recorded in
  `docs/CONVERSION_ACCELERATION_RESEARCH.md`; no accelerator implementation is
  implied by this research milestone.
- [x] Completed the Phase 3 implementation review. Exact end-to-end golden
  comparison remains an explicit Phase 5 integration dependency, and the
  deterministic scanline-palette difference is documented.
- [x] Added bounded `QImageReader` input for PNG, JPEG, BMP, GIF, and available
  TIFF/WebP plug-ins with explicit sRGB, alpha, orientation, and first-frame
  policies.
- [x] Added independent PCX and retro decoders for TI Artist, MSX SC2, Coleco
  CVPaint, Adam PowerPaint, and Adam HGR/HGRH layouts.
- [x] Routed file-open, command-line, clipboard, and drag-and-drop sources
  through the same cross-platform loader.
- [x] Added exact-pixel, format-routing, size-limit, malformed, and truncated
  input tests and documented the complete Phase 4 policy.
- [x] Added the platform-neutral export request and generated-file manifest.
- [x] Implemented and golden-tested RAW, RLE, TIFILES, V9T9, MSX SC2,
  Coleco CVPaint/ROM, Adam PowerPaint/HGR, and Extended BASIC writers.
- [x] Added Qt PNG export plus complete overwrite preflight and atomic writes.
- [x] Validated export applicability and filenames across all nine conversion
  layouts in the four-platform CI test matrix.
- [x] Completed and documented the Phase 5 export-format contract.
- [x] Completed the Phase 6 live workflow with responsive zoomable previews,
  debounced and generation-safe background conversion, presets, undo/reset,
  palette inspection, export summaries, persistent settings, shortcuts,
  accessibility metadata, and complete attribution.
- [x] Added an end-to-end interface test covering background conversion,
  stale-result rejection, scanline palettes, persistence, safe export, 125%
  scaling, and narrow/wide layouts; also verified native Windows rendering.
- [x] Replaced the action toolbar with File, View, and Help menus; added
  tabbed, horizontal, and vertical preview arrangements plus hideable adjacent
  and overlay Conversion-panel states.
- [x] Polished tabbed previews with non-redundant titles and bottom-right zoom
  controls; added overlay close, automatic mouse-leave hiding, and a right-edge
  reopen control; eliminated QML binding errors during application shutdown.
- [x] Added the New Convert 9918 logo as the cross-platform application icon,
  generated native multi-size icon assets, and watermarked empty preview panes.
- [x] Added immediate 256×192 source framing with compact positioning,
  palette-constrained background fill and eyedropper controls, plus selectable
  automatic or manual conversion updates.
- [x] Made pane and settings outlines theme-aware and consolidated each preview's
  zoom controls into a percentage menu with Fit/1:1 and stacked step buttons.
- [x] Consolidated Conversion-panel groups behind compact disclosure headers,
  with Art Style expanded initially and expert/output sections collapsed.
- [x] Added the headless `newconvert9918-cli` frontend with deterministic
  one-shot conversion/export, stable exit codes, JSON diagnostics, safe
  overwrite handling, and executable-level tests.

### Development environment

- [x] Use `C:\Users\Cisco\projects\NewConvert9918` as the local project root.
- [x] Keep build, test, packaging, and release commands reproducible from a
  normal local checkout.
- [x] Install a Qt 6 desktop development kit.
- [x] Install a Windows C++20 compiler (Qt MinGW 13.1).
- [x] Install CMake and Ninja with the Qt development tools.
- [x] Create the remote repository through GitHub's interface.
- [x] Run the first configure, build, and test cycle.

The Windows development kit is rooted at `C:\Qt`. The repository's Windows
preset uses MinGW Makefiles because Ninja process orchestration stalls in this
VM even though compilation itself succeeds. Linux and macOS retain Ninja as
the preferred generator. Zed remains optional: the build is defined entirely
by CMake and can run from any editor or terminal.

## Target architecture

```text
NewConvert9918/
├── app/                         Qt Quick application and adapters
│   └── qml/                     Windows, views, and reusable controls
├── include/newconvert9918/
│   ├── core/                    Public conversion API
│   └── formats/                 Public codec/export API
├── src/
│   ├── core/                    Scaling, palette, dithering, conversion
│   ├── formats/                 Retro input and output codecs
│   └── imageio/                 Qt image-I/O adapter
├── tests/
│   ├── unit/                    Focused algorithm and codec tests
│   ├── golden/                  Reference inputs and expected outputs
│   └── integration/             End-to-end conversion/export tests
├── docs/                        Architecture and migration records
├── packaging/                   Platform packaging configuration
└── .github/workflows/           Build, test, and release automation
```

### Architectural boundaries

- The core accepts an owned or viewed RGB/RGBA image buffer plus conversion
  settings and returns a conversion result.
- The core must not expose `HWND`, `HGLOBAL`, `BYTE`, `CString`, MFC classes,
  QML objects, or other platform-specific types.
- Qt translates `QImage`, clipboard content, dropped URLs, and UI state at the
  application boundary.
- Each retro file format has an independent reader or writer with unit tests.
- Exporters consume a completed conversion result; they do not read UI state.
- Long-running conversion work runs away from the UI thread and publishes only
  the newest requested preview.

## Delivery roadmap

### Phase 0 — Project foundation and provenance

- [x] Create the clean repository scaffold.
- [x] Record original and new-work attribution.
- [x] Preserve the original license terms.
- [x] Document that ImgSource will not be reused.
- [ ] Confirm the preferred display/copyright spelling for Cisco Garcia.
- [ ] Add `CODEOWNERS` after the GitHub owner/team names are known.
- [x] Create the public GitHub repository and push the scaffold.

**Exit criterion:** the project has an agreed license, traceable attribution,
a clean repository, and written dependency rules.

### Phase 1 — Buildable shell and development pipeline

- [x] Install Qt, CMake, Ninja, and the Windows compiler.
- [x] Configure the project with CMake.
- [x] Build the application shell.
- [x] Run the validation tests.
- [x] Resolve all compiler and QML warnings in the current shell.
- [x] Add formatting configuration for C++ and QML.
- [x] Add a GitHub Actions matrix for Windows, Ubuntu, Intel macOS, and Apple
  Silicon macOS.
- [x] Require the CI build and tests to pass before merging.

**Exit criterion:** a clean checkout configures, builds, and tests on all three
target operating systems.

### Phase 2 — Behavioral baseline

- [x] Preserve an untouched reference checkout of the original project.
- [x] Inventory every original control, option, conversion mode, input format,
  and output format.
- [x] Identify global state and Windows/MFC coupling in the original code.
- [x] Select representative test images:
  - [x] Photographic image with broad color range
  - [x] Pixel art with hard edges
  - [x] Cartoon/illustration with large flat regions
  - [x] Greyscale image
  - [x] Black-and-white image
  - [x] Portrait and landscape aspect ratios
  - [x] Very small and very large sources
  - [x] Transparency and malformed-file cases
- [x] Run each representative image through the original Windows executable.
- [x] Capture original preview images and every applicable binary export.
- [x] Capture default Bitmap 9918A previews plus TIFILES pattern and color
  tables for every valid source image.
- [x] Capture the remaining conversion modes with their applicable TIFILES
  pattern, color, multicolor, and palette tables for every valid source.
- [x] Record the original settings alongside each expected output.
- [x] Decide whether golden assets may be committed publicly.
- [x] Document known original defects separately from compatibility behavior.

**Exit criterion:** conversion correctness can be measured without relying on
visual memory or subjective comparison.

### Phase 3 — Portable conversion core

- [x] Define the initial conversion-mode and dither-mode enums.
- [x] Define initial conversion settings and validation.
- [x] Define `RgbImage`, pixel format, row stride, and size-limit rules.
- [x] Define `ConversionRequest`, `ConversionResult`, and diagnostic types.
- [x] Define palette and target-memory-table types.
- [x] Implement scaling/crop positioning independently from conversion.
- [x] Implement color-space and distance calculations.
- [x] Implement histogram stretching and gamma correction.
- [x] Implement palette selection and median-cut behavior.
- [x] Implement error-distribution kernels and ordered dithering.
- [x] Port conversion modes in this order:
  - [x] Bitmap 9918A / Graphics II
  - [x] Greyscale Bitmap 9918A
  - [x] Black-and-White Bitmap 9918A
  - [x] Bitmap Color Only 9918A
  - [x] Multicolor 9918
  - [x] Dual Multicolor 9918
  - [x] Half Multicolor 9918A
  - [x] Paletted Bitmap F18A
  - [x] Scanline Palette Bitmap F18A
- [x] Add cancellation and generation IDs for interactive preview jobs.
- [x] Measure conversion performance and memory use.

**Exit criterion:** every conversion mode matches approved golden output or has
a documented, reviewed reason for differing.

**Exit review:** implementation is complete. Raw core table contracts are
covered, Phase 4 supplies source decoding/scaling, and Phase 5 verifies export
framing from approved captured payloads. Full source-to-target golden parity
remains a separate review item. See `docs/PHASE3_COMPATIBILITY.md`.

### Phase 4 — Image input and source formats

- [x] Use `QImageReader` for PNG, JPEG, BMP, and GIF.
- [x] Use Qt Image Formats for TIFF and WebP where available.
- [x] Confirm color-space, alpha, orientation, and animation-frame policies.
- [x] Implement or select a permissively licensed PCX decoder.
- [x] Implement independently testable retro readers:
  - [x] TI Artist
  - [x] MSX SC2
  - [x] Coleco CVPaint (`.pc`)
  - [x] Adam PowerPaint (`.pp`)
  - [x] Adam HGR/HGRH
- [x] Add explicit allocation and input-size limits.
- [x] Add malformed and truncated input tests.
- [x] Add cross-platform clipboard image input.
- [x] Add cross-platform drag-and-drop input.

**Exit criterion:** supported source images load consistently and safely on
Windows, Linux, and macOS without ImgSource.

**Exit review:** complete. Common, PCX, and retro inputs share a bounded
`RgbImage` boundary; all interactive paths use the same loader. Policies and
format contracts are recorded in `docs/IMAGE_INPUT.md` and exercised in the
cross-platform CTest matrix.

### Phase 5 — Export formats

- [x] Define a common export request and generated-file manifest.
- [x] Implement RAW pattern, color, and palette tables.
- [x] Implement the original RLE encoding.
- [x] Implement TIFILES headers.
- [x] Implement V9T9 headers.
- [x] Implement MSX SC2 output.
- [x] Implement Coleco CVPaint output.
- [x] Implement Adam PowerPaint output.
- [x] Implement Adam HGR output.
- [x] Implement ColecoVision ROM output.
- [x] Implement Extended BASIC program output.
- [x] Implement PNG preview/export with `QImageWriter`.
- [x] Validate which conversion modes apply to each export format.
- [x] Warn clearly when a requested export would overwrite existing files.
- [x] Test every generated filename and byte layout on all platforms.

**Exit criterion:** every supported export is deterministic and compatible
with its intended emulator, computer, console, or downstream tool.

**Exit review:** complete. Portable exporters construct all bytes before I/O;
Qt supplies PNG encoding and conflict-safe atomic writes. Golden captures pin
the legacy layouts, and injected validated loader templates keep opaque
upstream machine code out of production. See `docs/EXPORT_FORMATS.md`.

### Phase 6 — Redesigned interface and workflow

- [x] Create the first split-preview Qt Quick shell.
- [x] Establish the primary flow:
  **Open → crop/scale → choose mode → adjust → preview → export**.
- [x] Add source and converted-image zoom, pan, fit, and 1:1 controls.
- [x] Add crop/fill positioning with immediate visual feedback.
- [x] Replace the manual Reload button with debounced background conversion.
- [x] Prevent stale background results from replacing newer previews.
- [x] Add an optional persisted progressive preview that publishes real
  completed rows without adding work when disabled.
- [x] Group common settings separately from advanced settings.
- [x] Add named presets for recommended dithering configurations.
- [x] Add undo/reset for settings changes.
- [x] Add palette inspection and optional scanline-palette visualization.
- [x] Add output-size and generated-file summaries before export.
- [x] Persist settings using Qt's cross-platform settings API.
- [x] Add useful keyboard shortcuts.
- [x] Add accessible names, keyboard navigation, and sufficient contrast.
- [x] Test high-DPI and fractional display scaling.
- [x] Test narrow and wide window layouts.
- [x] Add an About view containing the full attribution and license links.

**Exit criterion:** common conversions are understandable without consulting
documentation, while advanced controls remain available to expert users.

**Exit review:** complete. The UI now drives the same portable conversion and
manifest layers tested by the core suites, while cancellable generation-tagged
workers keep interaction live and prevent stale publication. Common controls
remain visible, expert controls collapse, and the workflow is verified at
narrow, wide, and fractional-scale layouts. See `docs/INTERFACE_WORKFLOW.md`.

### Phase 6A — Legacy feature parity and expert controls

The original feature inventory is recorded in `docs/BEHAVIORAL_BASELINE.md`.
This phase tracks the remaining gap between that inventory and functionality
that an end user can actually reach in New Convert 9918. A portable algorithm
or writer is not considered complete here until its required settings,
resources, validation, and interface are also available.

#### Already ported or intentionally superseded

- [x] Expose all nine original TMS9918A and F18A conversion modes.
- [x] Expose all six scaling filters and all four fit/crop positions.
- [x] Provide source positioning, including one-pixel nudging and centering.
- [x] Expose histogram stretch, perceptual matching, gamma, luma emphasis,
  maximum color shift, flicker difference, and ordered brightness.
- [x] Provide Floyd–Steinberg, Atkinson, Pattern, Diagonal, None, Ordered, and
  Ordered-with-error choices.
- [x] Replace the original stale manual-Reload workflow with cancellable Auto
  conversion and an explicit Update command; provide File → Reload separately
  to reread a file-backed source.
- [x] Replace modifier-key TI Artist loading with automatic bounded format and
  header detection.
- [x] Provide active-palette swatches and optional scanline-palette inspection.
- [x] Expose TIFILES, V9T9, RAW, RLE, MSX SC2, Coleco CVPaint, Adam
  PowerPaint, Adam HGR/HGRH, and PNG export.
- [x] Persist every setting currently exposed by the application.

#### Remaining parity work

- [x] Add an Average/Accumulate error-handling control and persist it; both
  algorithms already exist in the portable core.
- [x] Add a 2×2/4×4 ordered-dither selector and map the four original Order
  combinations clearly; both threshold maps already exist in the core.
- [x] Add editable six-cell error-distribution weights with validation,
  presets, persistence, and focused conversion tests.
- [x] Correct the ordered-brightness UI range to the core-supported 0–16 range
  and verify its brighten/darken semantics against the original executable.
- [x] Expose and persist perceptual red/green/blue weights, including a
  one-click restore to the audited 30/52/18 defaults.
- [x] Restore the full original 0–100% maximum-color-shift UI range while
  retaining compact controls for the commonly useful lower values.
- [x] Keep portable one-pixel source nudging without hidden modifier
  acceleration or the original seven-pixel horizontal cap; this intentional
  interaction difference is recorded in the Phase 6A review.
- [x] Defer Toon/restricted-color matching until independent executable
  fixtures define it; do not expose an unverified compatibility claim.
- [x] Add editable working-palette support, default restoration, persistence,
  and palette-aware golden cases without importing the original INI format.
- [x] Expose Median Cut versus Popularity F18A palette selection; both global
  selection algorithms already exist in the core.
- [x] Implement F18A scanline static-color count and Region 1/2/3 inclusion,
  including stability tests for horizontal palette banding.
- [x] Add the PowerPaint 240×160 framing option so source preparation matches
  the already-implemented PowerPaint reader and writer.
- [x] Keep ColecoVision ROM, Extended BASIC, and Extended BASIC RLE export at
  the tested library boundary until independently built or separately
  authorized loader templates are supplied; no legacy machine code is bundled.
- [x] Approve the hash-pinned original captures as the behavioral oracle and
  document the clean implementation's intentional output differences in
  `docs/PHASE6A_PARITY_REVIEW.md`.

#### Intentional exclusions or post-v1 decisions

- [x] Keep the original broken ColecoVision RLE cartridge writer unsupported.
- [x] Keep `.jpc` unsupported because the audited application indexes it but
  contains no corresponding decoder.
- [x] Defer random-folder slideshow/Next mode,
  recursive image indexing, and automatic clipboard polling.
- [x] Defer cross-instance filename synchronization; any later implementation
  requires a new portable and secure design.

**Exit criterion:** every approved legacy conversion control is either exposed
and tested end to end or recorded as an explicit intentional difference. Core-
only exporters are not described as user-facing until their legal loader
resources and interface paths are complete.

**Exit review:** complete. See `docs/PHASE6A_PARITY_REVIEW.md` for implemented
controls, compatibility-gated exclusions, and intentional interaction changes.

### Phase 7 — Command line and automation

- [x] Define a stable command-line syntax independent of the GUI.
- [x] Support one-shot input, conversion preset, and export operations.
- [x] Return meaningful exit codes and machine-readable diagnostics.
- [x] Keep GUI and CLI behavior on the same conversion core.
- [x] Add end-to-end CLI tests suitable for continuous integration.

**Exit review:** complete. The desktop controller and headless executable share
one synchronous transform/adjust/convert pipeline. The CLI uses the same
bounded loaders, manifest exporters, and atomic writer as the desktop, never
prompts, and reports stable JSON or human-readable output. See
`docs/COMMAND_LINE.md`.

**Exit criterion:** repeatable conversions can run without a graphical session.

### Phase 8 — Packaging and releases

The detailed artifact matrix, self-contained runtime contract, preview/update
policy, signing policy, GitHub workflow, clean-system tests, and operator
checklist are defined in `docs/RELEASE_PROCESS.md`. The shared-library Qt model
is the release standard; users must not need Qt, CMake, a compiler, or another
development environment.

The first unsigned cross-platform package set was published as
`v0.1.0-beta.1`, followed by `v0.1.0-beta.2`. Both Debian packages incorrectly
installed an AppImage runtime into global system paths; a global
`/usr/bin/qt.conf` redirected unrelated Qt applications to the package's Qt 6
plugins and prevented Kubuntu's Qt 5 SDDM greeter from starting after reboot.
The AppImage and non-Linux assets were not affected by that path collision.
`v0.1.0-beta.3` is the published corrective isolation release. Its tagged
workflow passed the Debian archive-layout, installation, launch, forbidden-path,
removal, and cleanup gates. Signing and notarization remain required for the
later official stable release.

#### Preview prerelease milestone

- [x] Make CMake the single version source for GUI, CLI, package metadata,
  tags, asset names, and release titles.
- [x] Freeze the Windows installer ID, macOS bundle ID, Linux application ID,
  package/executable names, install paths, and Qt settings identity before the
  first beta.
- [x] Build unsigned Windows setup/portable, Linux AppImage/DEB, and Intel and
  Apple Silicon macOS DMG preview assets from clean CI checkouts.
- [x] Include licenses, notices, and verified SHA-256 checksums, and audit the
  staged runtime content for every preview package.
- [x] Smoke-test packaged launch and representative conversion/export on clean
  CI runners, and record hands-on Windows and Linux package feedback.
- [x] Publish `v0.1.0-beta.1` as a GitHub prerelease with platform guidance,
  unsigned-package authorization instructions, known issues, and manual update
  instructions.
- [x] Publish the corrective `v0.1.0-beta.2` prerelease with a Windows GUI
  subsystem check and a documented Debian terminal-install fallback.
- [x] Publish `v0.1.0-beta.3` with the bundled Debian runtime confined to
  `/opt/newconvert9918`, positive `Installed-Size` metadata, forbidden-path
  audits, installed-launch tests, and complete removal verification.
- [x] Add a release-blocking Debian layout audit that rejects global
  `/usr/bin/qt.conf`, `/usr/plugins`, `/usr/qml`, `/apprun-hooks`, and bundled
  Qt/X11 libraries placed directly in `/usr/lib`.
- [ ] Verify settings and recipe persistence plus beta replacement/upgrade on
  clean target systems before the stable release.
- [ ] Add a notification-only `Help -> Check for Updates` flow before a later
  prerelease or stable release, with stable/preview/off channels; do not block
  the first beta on this feature.
- [ ] Add validated AppStream MetaInfo using the frozen Linux application ID,
  with matching desktop file, icon, screenshots, release data, URLs, and
  license declarations.
- [ ] Evaluate and acceptance-test a Snap beta channel as the first
  Discover-visible preview option while retaining direct AppImage and isolated
  Debian downloads; target Flatpak/Flathub after a stable source-build and
  sandbox contract is ready.
- [x] Defer automatic executable download and installation until signed update
  verification, recovery, rollback, and migration behavior are designed and
  tested.

**Preview milestone review:** `v0.1.0-beta.3` was published after its clean
tagged workflow passed the new private-runtime, forbidden-path, installed-launch,
uninstall, and cleanup gates. The earlier Kubuntu incident showed that automated
application launch alone does not verify the desktop login path. A recorded
clean-Kubuntu install plus reboot/logout, SDDM login, GUI/CLI launch, and removal
test remains required before closing this milestone again.

#### Stable release completion

- [ ] Add Qt's QML deployment install script and stage GUI/CLI runtime
  dependencies from CMake install rules.
- [ ] Windows x64: produce a signed NSIS installer and a portable ZIP from one
  audited staged tree.
- [ ] Linux x64: produce an AppImage and a Debian package after pinning and
  testing the oldest supported Linux/glibc baseline.
- [ ] Linux ARM64/AArch64: add a native ARM64 CI build, AppImage, isolated
  `arm64` Debian package, architecture/dependency audits, and clean-system
  install, launch, desktop-session, upgrade, removal, and host-integrity tests.
- [ ] Parameterize Linux runner, Qt host/architecture, linuxdeploy tool,
  Debian `Architecture`, asset naming, and final manifest without weakening
  the existing x64 or filesystem-isolation gates.
- [ ] Establish the ARM64 minimum glibc from the actual Qt/runtime build; use
  the x64 Ubuntu 22.04 baseline only if native clean-system tests prove it.
- [ ] macOS: produce separately tested, Developer ID-signed and notarized Intel
  and Apple Silicon DMGs; reconsider a universal bundle only after both native
  packages are established.
- [ ] Bundle only required Qt/QML, platform, image-format, accessibility,
  style, and compiler runtime files; reject debug or unresolved dependencies.
- [ ] Include `LICENSE`, `NOTICE.md`, Qt license text, third-party notices, and
  available SBOM material in every applicable package.
- [ ] Confirm original-author redistribution permission covers public binary
  releases and these package formats.
- [ ] Define and enforce minimum Windows, macOS, and Linux versions and CPU
  architectures in CMake, CI, package metadata, and release notes.
- [ ] Add a protected, tag-driven GitHub Actions workflow that builds, tests,
  deploys, packages, signs, smoke-tests, generates SHA-256 checksums, and creates
  a draft GitHub Release.
- [ ] Test installation/portable launch, first launch, representative input,
  conversion, export, recipe round-trip, CLI operation, upgrade, and removal on
  clean supported systems.
- [ ] Publish prerelease/stable notes with asset guidance, compatibility
  differences, known issues, signing state, checksums, and support boundaries.

**Exit criterion:** users on each target OS can download, launch, convert, and
export without installing a development environment, and a maintainer can
reproduce and publish the complete release from a clean tag by following
`docs/RELEASE_PROCESS.md` without undocumented workstation state.

### Phase 9 — GitHub governance

- [x] Create `CiscoGarciaFL/NewConvert9918` as a public repository.
- [x] Add the local remote and push `main`.
- [ ] Keep organization base permissions read-only.
- [ ] Grant Write or Maintain access only to approved people or teams.
- [x] Protect `main` with a repository ruleset:
  - [x] Require pull requests
  - [x] Require at least one approval
  - [x] Dismiss stale approvals after new changes
  - [x] Require Windows, Linux, macOS, and test status checks
  - [x] Block force pushes
  - [x] Block branch deletion
- [ ] Add issue and pull-request templates.
- [ ] Add `CODE_OF_CONDUCT.md` if outside discussion is enabled.
- [ ] Document how uninvited contributions will be handled.
- [ ] Enable dependency and secret scanning where available.

**Exit criterion:** the code is publicly visible, but only approved maintainers
can push or merge changes.

### Post-v1 proposal — Batch Mode

Video, animated GIF, slideshow, and numbered-image sources will eventually be
normalized into enumerated still frames and processed with one frozen snapshot
of the ordinary conversion settings. This is new functionality rather than
legacy parity and remains documentation-only until the media backend,
licensing, packaging, limits, and frame/timing contract are approved. See
`docs/BATCH_MODE.md`.

### Post-v1 proposal — 9918 and F18A Design Tools

A separate hardware-aware authoring workspace will support original TMS9918A
and enhanced F18A sprite patterns, arbitrarily arranged composite sprites,
accurate scanline/collision diagnostics, one-to-three-bank character viewing
and editing, pattern reservations and stable image allocation, and Multicolor
simulation. This remains documentation-only until the hardware contracts,
project format, fixtures, and implementation phase are approved. See
`docs/9918_DESIGN_TOOLS.md`.

## Release milestones

### v0.1 — First proven conversion

- [x] Buildable on all target platforms.
- [x] Load PNG/JPEG/BMP.
- [x] Convert to Bitmap 9918A.
- [x] Show a live converted preview.
- [x] Export RAW pattern and color tables.
- [x] Match the original golden outputs.

### v0.5 — Functional preview

- [x] All conversion modes implemented.
- [x] Common and retro source formats implemented.
- [x] Primary export formats implemented.
- [x] Redesigned workflow usable end-to-end.
- [x] CLI conversion available.

### v1.0 — Public cross-platform release

- [ ] Golden-output suite approved.
- [ ] Windows, Linux, and macOS packages tested.
- [ ] Documentation and attribution complete.
- [ ] No critical known conversion or data-loss defects.
- [ ] Repository governance and release automation active.

## Quality gates

A task is not complete merely because it compiles. Apply the relevant gates:

- [ ] New behavior has focused automated tests.
- [ ] Conversion changes have golden-output coverage.
- [ ] Binary writers have byte-layout tests.
- [ ] Malformed inputs fail safely with a useful message.
- [ ] UI conversion work does not block the main thread.
- [ ] No platform-specific type leaks into the core API.
- [ ] No ImgSource code, binaries, keys, or headers are included.
- [ ] New dependencies have compatible licenses and recorded notices.
- [ ] Windows, Linux, and macOS CI remains green.
- [ ] User-visible changes update documentation or release notes.

## Known risks

| Risk | Mitigation |
| --- | --- |
| Conversion logic is entangled with MFC and global state | Port behind a small testable API, one mode at a time. |
| Original output may be difficult to reproduce exactly | Capture golden files before refactoring algorithms. |
| ImgSource is unavailable and has uncertain redistribution rights | Replace it; never copy or ship it. |
| PCX and retro formats lack built-in Qt support | Isolate small codecs and test malformed inputs. |
| Interactive conversion may stall the interface | Use cancellable background jobs and discard stale results. |
| Cross-platform packaging may differ substantially | Build CI and packaging early rather than at the end. |
| Public repository permits forks and external PRs | Restrict write/merge roles and protect `main`. |
| Dependency licenses could conflict with the original terms | Review every dependency before adoption; exclude GPL-only code. |

## Decision log

| Date | Decision | Reason |
| --- | --- | --- |
| 2026-09-14 | Start with clean history | The new architecture and interface constitute a fresh cross-platform project. |
| 2026-09-14 | Use Qt Quick Controls 2 | UI and workflow improvement are primary project goals. |
| 2026-09-14 | Keep a standard C++ core | Enables testing, CLI use, and platform independence. |
| 2026-09-14 | Replace ImgSource | The product is retired and its redistribution position is unsuitable. |
| 2026-09-14 | Preserve the original custom license | Agreed with the original author; full attribution remains mandatory. |
| 2026-09-14 | Use golden-output testing | The original application's output quality is the compatibility baseline. |
| 2026-09-14 | Keep Zed optional and drive builds with CMake presets | The same repository workflow must work from Zed, another editor, or a terminal. |
| 2026-09-14 | Use native open toolchains on each platform | Qt MinGW avoids an MSVC dependency on Windows; GCC/Clang and AppleClang fit Linux and macOS. |
| 2026-09-14 | Use MinGW Makefiles for the current Windows VM | Both available Ninja binaries stall during CMake's compiler probe in this VM. |
| 2026-09-15 | Test Intel and Apple Silicon macOS separately in CI | Native jobs catch architecture-specific failures before universal release packaging. |
| 2026-09-15 | Protect `main` with reviews and four platform CI checks | Keeps direct changes off the release branch and makes the cross-platform promise enforceable. |
| 2026-09-15 | Keep the audited original at commit `edbdf0f` in an external sibling checkout | Provides a stable behavioral reference without bringing original or ImgSource-related files into the clean implementation. |
| 2026-09-15 | Use project-created image-generation output and deterministic procedural fixtures for the public corpus | Covers photographic and synthetic edge cases without copying upstream, ImgSource, or third-party stock assets. |
| 2026-09-15 | Preserve default original outputs by hash and fix unsafe behavior intentionally | Byte-level output is the parity baseline, while confirmed path handling and misleading broken-format behavior are not copied. |
| 2026-09-17 | Normalize all image inputs through a bounded `RgbImage` loader | Common, PCX, retro, clipboard, and drop paths now share color, alpha, safety, and error behavior on every platform. |
| 2026-09-17 | Keep ROM and Extended BASIC machine code in validated caller-supplied templates | Completes deterministic patching and golden verification without embedding opaque upstream binaries in the portable implementation. |
| 2026-09-17 | Preflight complete export manifests before atomic Qt writes | Users see every collision before any output is changed, avoiding partial or silent overwrites. |
| 2026-09-17 | Debounce UI conversion and publish only the newest generation | Keeps the interface responsive while guaranteeing that rapid settings changes cannot display stale output. |
| 2026-09-17 | Use the Qt Fusion control style for the application | Provides consistent contrast and control rendering across Windows, Linux, and macOS while retaining platform font and DPI behavior. |
| 2026-09-18 | Separate source framing feedback from conversion updates | Position and background-fill edits remain immediate while Auto or an explicit Update controls expensive conversion work. |
| 2026-09-19 | Share one synchronous conversion pipeline between the desktop and CLI | Prevents headless automation from drifting away from the transform, adjustment, palette, and conversion behavior users see in the application. |
| 2026-09-20 | Optimize the exact CPU search before attempting a GPU backend | Shared-prefix reduction, runtime SIMD, and independent-work threading can improve every platform while preserving a scalar oracle; a fixed-point numeric contract is the gate for portable GPU work. |
| 2026-09-20 | Model 9918/F18A design tools as a hardware-profiled project workspace | Sprite, character, allocation, and Multicolor editors must share one validated memory model so previews expose real chip limits and exports cannot drift from what the user designed. |
| 2026-09-27 | Keep GitHub AppImage and isolated Debian downloads for the current Linux beta; add AppStream metadata, evaluate a Snap beta channel for Discover, and target Flatpak/Flathub for stable cross-distribution delivery | Discover installs from configured repository and store backends rather than turning an arbitrary local `.deb` into a catalog application; each new channel needs its own sandbox, update, authorization, and clean-system acceptance tests. |
| 2026-09-27 | Plan native Linux ARM64/AArch64 AppImage and Debian artifacts using the existing private-runtime packaging model; treat ARM32 as a separate target | The portable C++/Qt design and pinned deployment tools can support AArch64, but architecture-specific runners, Qt binaries, metadata, dependency audits, glibc verification, and clean-system tests are required before support is advertised. |

## Next session checklist

When opening this project again:

1. [ ] Open `C:\Users\Cisco\projects\NewConvert9918` as the local project.
2. [ ] Read this file and `docs/ARCHITECTURE.md`.
3. [ ] Run `git status --short --branch` and `git log --oneline -5`.
4. [ ] Confirm the platform preset and required Qt toolchain are available.
5. [ ] Configure, build, and test with the matching CMake preset.
6. [ ] Update the Current Status section with any environment changes.
9. [x] Begin Phase 2's feature inventory and golden-output corpus.
10. [x] Select and license the representative Phase 2 source-image corpus.
11. [x] Capture default Bitmap 9918A previews and TIFILES exports from the
    original Windows executable for each valid corpus image.
12. [x] Expand command-line reference capture across the remaining conversion
    modes and their applicable TIFILES outputs.
13. [x] Capture applicable non-TIFILES export formats through a controlled,
    reproducible Windows UI workflow.
14. [x] Begin Phase 3 with the portable `RgbImage` model, explicit row-stride
    semantics, and checked image-allocation limits.
15. [x] Define `ConversionRequest`, `ConversionResult`, and diagnostic types.
16. [x] Define palette and target-memory-table types.
17. [x] Implement scaling and crop positioning independently from conversion.
18. [x] Implement color-space and distance calculations.
19. [x] Implement histogram stretching and gamma correction.
20. [x] Implement palette selection and median-cut behavior.
21. [x] Implement error-distribution kernels and ordered dithering.
22. [x] Port Bitmap 9918A / Graphics II conversion mode.
23. [x] Port Greyscale Bitmap 9918A conversion mode.
24. [x] Port Black-and-White Bitmap 9918A conversion mode.
25. [x] Port Bitmap Color Only 9918A conversion mode.
26. [x] Port Multicolor 9918 conversion mode.
27. [x] Port Dual Multicolor 9918 conversion mode.
28. [x] Port Half Multicolor 9918A conversion mode.
29. [x] Port Paletted Bitmap F18A conversion mode.
30. [x] Port Scanline Palette Bitmap F18A conversion mode.
31. [x] Add cancellation and generation IDs for interactive preview jobs.
32. [x] Measure conversion performance and memory use.
33. [x] Record the Phase 3 compatibility and exit review.
34. [x] Complete Phase 4 image input and source formats.
35. [x] Complete Phase 5 export formats and generated-file manifests.
36. [x] Complete Phase 6 live conversion and export workflow integration.
37. [x] Complete Phase 7 command-line syntax, one-shot conversion/export,
    JSON diagnostics, stable exit codes, and executable-level tests.
38. [x] Begin Phase 8 cross-platform packaging and clean-system release tests.

Suggested opening prompt:

> Continue New Convert 9918 from PROJECT_PLAN.md. Verify the repository and
> toolchain state, update the checklist, and complete the next unchecked task
> without changing established architecture or license decisions.
