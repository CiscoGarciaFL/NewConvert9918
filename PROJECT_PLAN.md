# New Convert 9918 — Project Plan and Checklist

Last updated: 2026-09-16

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
- [x] Ran the validation suite successfully (1/1 tests passing).
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

### Development environment

- [x] Add `C:\Users\Cisco\projects\NewConvert9918` as a local project in the
  Codex desktop app.
- [x] Start future Codex tasks from that project in **Local** mode.
- [x] Select **Ask for approval** or **Full access** for tasks that must write
  Git metadata.
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
- [ ] Capture original preview images and every applicable binary export.
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
- [ ] Port conversion modes in this order:
  - [x] Bitmap 9918A / Graphics II
  - [x] Greyscale Bitmap 9918A
  - [x] Black-and-White Bitmap 9918A
  - [x] Bitmap Color Only 9918A
  - [x] Multicolor 9918
  - [ ] Dual Multicolor 9918
  - [ ] Half Multicolor 9918A
  - [ ] Paletted Bitmap F18A
  - [ ] Scanline Palette Bitmap F18A
- [ ] Add cancellation and generation IDs for interactive preview jobs.
- [ ] Measure conversion performance and memory use.

**Exit criterion:** every conversion mode matches approved golden output or has
a documented, reviewed reason for differing.

### Phase 4 — Image input and source formats

- [ ] Use `QImageReader` for PNG, JPEG, BMP, and GIF.
- [ ] Use Qt Image Formats for TIFF and WebP where available.
- [ ] Confirm color-space, alpha, orientation, and animation-frame policies.
- [ ] Implement or select a permissively licensed PCX decoder.
- [ ] Implement independently testable retro readers:
  - [ ] TI Artist
  - [ ] MSX SC2
  - [ ] Coleco CVPaint (`.pc`)
  - [ ] Adam PowerPaint (`.pp`)
  - [ ] Adam HGR/HGRH
- [ ] Add explicit allocation and input-size limits.
- [ ] Add malformed and truncated input tests.
- [ ] Add cross-platform clipboard image input.
- [ ] Add cross-platform drag-and-drop input.

**Exit criterion:** supported source images load consistently and safely on
Windows, Linux, and macOS without ImgSource.

### Phase 5 — Export formats

- [ ] Define a common export request and generated-file manifest.
- [ ] Implement RAW pattern, color, and palette tables.
- [ ] Implement the original RLE encoding.
- [ ] Implement TIFILES headers.
- [ ] Implement V9T9 headers.
- [ ] Implement MSX SC2 output.
- [ ] Implement Coleco CVPaint output.
- [ ] Implement Adam PowerPaint output.
- [ ] Implement Adam HGR output.
- [ ] Implement ColecoVision ROM output.
- [ ] Implement Extended BASIC program output.
- [ ] Implement PNG preview/export with `QImageWriter`.
- [ ] Validate which conversion modes apply to each export format.
- [ ] Warn clearly when a requested export would overwrite existing files.
- [ ] Test every generated filename and byte layout on all platforms.

**Exit criterion:** every supported export is deterministic and compatible
with its intended emulator, computer, console, or downstream tool.

### Phase 6 — Redesigned interface and workflow

- [x] Create the first split-preview Qt Quick shell.
- [x] Establish the primary flow:
  **Open → crop/scale → choose mode → adjust → preview → export**.
- [ ] Add source and converted-image zoom, pan, fit, and 1:1 controls.
- [ ] Add crop/fill positioning with immediate visual feedback.
- [ ] Replace the manual Reload button with debounced background conversion.
- [ ] Prevent stale background results from replacing newer previews.
- [ ] Group common settings separately from advanced settings.
- [ ] Add named presets for recommended dithering configurations.
- [ ] Add undo/reset for settings changes.
- [ ] Add palette inspection and optional scanline-palette visualization.
- [ ] Add output-size and generated-file summaries before export.
- [ ] Persist settings using Qt's cross-platform settings API.
- [ ] Add useful keyboard shortcuts.
- [ ] Add accessible names, keyboard navigation, and sufficient contrast.
- [ ] Test high-DPI and fractional display scaling.
- [ ] Test narrow and wide window layouts.
- [ ] Add an About view containing the full attribution and license links.

**Exit criterion:** common conversions are understandable without consulting
documentation, while advanced controls remain available to expert users.

### Phase 7 — Command line and automation

- [ ] Define a stable command-line syntax independent of the GUI.
- [ ] Support one-shot input, conversion preset, and export operations.
- [ ] Return meaningful exit codes and machine-readable diagnostics.
- [ ] Keep GUI and CLI behavior on the same conversion core.
- [ ] Add end-to-end CLI tests suitable for continuous integration.

**Exit criterion:** repeatable conversions can run without a graphical session.

### Phase 8 — Packaging and releases

- [ ] Windows: package the application and shared Qt runtime.
- [ ] Linux: choose AppImage, Flatpak, or both after compatibility testing.
- [ ] macOS: create an application bundle and test Intel/Apple Silicon policy.
- [ ] Bundle only the required Qt and image-format plugins.
- [ ] Include `LICENSE`, `NOTICE.md`, and third-party notices in every package.
- [ ] Generate checksums for release artifacts.
- [ ] Decide whether code signing/notarization is required for each platform.
- [ ] Test installation, first launch, conversion, and export on clean systems.
- [ ] Publish release notes including compatibility differences and known issues.

**Exit criterion:** users on each target OS can download, launch, convert, and
export without installing a development environment.

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

## Release milestones

### v0.1 — First proven conversion

- [ ] Buildable on all target platforms.
- [ ] Load PNG/JPEG/BMP.
- [ ] Convert to Bitmap 9918A.
- [ ] Show a live converted preview.
- [ ] Export RAW pattern and color tables.
- [ ] Match the original golden outputs.

### v0.5 — Functional preview

- [ ] All conversion modes implemented.
- [ ] Common and retro source formats implemented.
- [ ] Primary export formats implemented.
- [ ] Redesigned workflow usable end-to-end.
- [ ] CLI conversion available.

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

## Next session checklist

When opening this project again:

1. [ ] Open `C:\Users\Cisco\projects\NewConvert9918` as the Codex project.
2. [ ] Use the **Local** environment.
3. [ ] Select **Ask for approval** or **Full access** if Codex should commit.
4. [ ] Read this file and `docs/ARCHITECTURE.md`.
5. [ ] Run `git status --short --branch` and `git log --oneline -5`.
6. [ ] Confirm the platform preset and required Qt toolchain are available.
7. [ ] Configure, build, and test with the matching CMake preset.
8. [ ] Update the Current Status section with any environment changes.
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
27. [ ] Port Dual Multicolor 9918 conversion mode.

Suggested opening prompt:

> Continue New Convert 9918 from PROJECT_PLAN.md. Verify the repository and
> toolchain state, update the checklist, and complete the next unchecked task
> without changing established architecture or license decisions.
