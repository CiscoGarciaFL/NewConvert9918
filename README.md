# RetroVDP Studio

<p align="center">
  <img src="app/assets/RetroVDPStudio.svg" width="220" alt="RetroVDP Studio logo">
</p>

RetroVDP Studio is a cross-platform graphics workspace for classic video
display processors. It combines source preparation and conversion with
hardware-aware screen, character, pattern, palette, and sprite tools.

The application is organized around three independent ideas:

1. **Sources are reusable.** Common images and supported retro formats can be
   opened as source material without being restricted to the hardware or
   software that originally created them.
2. **Targets own their output.** Each selected VDP profile applies its own
   display modes, palettes, memory organization, sprite limits, validation,
   and export formats. A future project may retain several managed target
   outputs derived from the same source.
3. **Native imports preserve structure only when it is meaningful.** Pattern,
   sprite, tile-map, palette, and other editors offer direct import only when
   the source data can be represented by the active target. Other supported
   files remain available through the visual conversion path.

RetroVDP Studio is inspired by
[Convert9918](https://github.com/tursilion/convert9918), created by Mike Brent
(Tursi). Convert9918 remains a historical inspiration and behavioral reference;
RetroVDP Studio has its own product identity and a broader multi-target design.

## Status

The current release implements complete TMS9918A and F18A conversion profiles,
while the target-profile architecture is intended to add V9938 and other
classic VDPs without another product redesign. The input pipeline loads common
Qt raster formats, PCX, and supported retro formats with explicit safety
limits. The export layer provides deterministic RAW, RLE,
TIFILES, V9T9, MSX, Coleco, Adam, Extended BASIC, ROM, and PNG exporters with
generated-file manifests and overwrite preflight. The Qt workspace provides
menu-driven tabbed/horizontal/vertical previews, an adjacent or overlay Side
Panel, debounced background conversion,
presets, undo/reset, palette inspection, persistent settings, export summaries,
and accessible narrow/wide layouts. It also provides a headless command-line
frontend for deterministic one-shot conversion and export, stable exit codes,
JSON diagnostics, and automation-friendly overwrite handling.

See [`PROJECT_PLAN.md`](PROJECT_PLAN.md) for the durable roadmap, current
status, acceptance criteria, and next-session checklist. The original
application's audited behavior, formats, controls, and portability boundaries
are recorded in [`docs/BEHAVIORAL_BASELINE.md`](docs/BEHAVIORAL_BASELINE.md).
Confirmed original defects and compatibility policy are tracked separately in
[`docs/ORIGINAL_DEFECTS.md`](docs/ORIGINAL_DEFECTS.md).
The approved, redistribution-safe source-image corpus and its provenance are
documented in [`tests/golden/README.md`](tests/golden/README.md).
The optimized timing and owned-buffer baseline is in
[`docs/PERFORMANCE.md`](docs/PERFORMANCE.md). Phase 3 compatibility decisions
and the remaining end-to-end golden dependency are recorded in
[`docs/PHASE3_COMPATIBILITY.md`](docs/PHASE3_COMPATIBILITY.md).
The CPU-algorithm, SIMD, multicore, and portable-GPU acceleration study—with
benchmark-based speed ranges and a gated implementation sequence—is in
[`docs/CONVERSION_ACCELERATION_RESEARCH.md`](docs/CONVERSION_ACCELERATION_RESEARCH.md).
Image formats, color/alpha/orientation policy, and input limits are documented
in [`docs/IMAGE_INPUT.md`](docs/IMAGE_INPUT.md).
Export applicability, byte layouts, loader-template policy, and overwrite
behavior are documented in [`docs/EXPORT_FORMATS.md`](docs/EXPORT_FORMATS.md).
The completed live-preview workflow, controls, shortcuts, responsive behavior,
and interface verification are documented in
[`docs/INTERFACE_WORKFLOW.md`](docs/INTERFACE_WORKFLOW.md).
The headless syntax, supported names, JSON contract, and exit codes are in
[`docs/COMMAND_LINE.md`](docs/COMMAND_LINE.md).
The cross-platform artifact matrix, standalone-runtime promise, preview and
update policy, signing policy, GitHub Release automation, and clean-system
release checklist are defined in
[`docs/RELEASE_PROCESS.md`](docs/RELEASE_PROCESS.md).
The post-v1 design for enumerating video, animated-image, slideshow, and still
sequence sources into deterministic conversion jobs is in
[`docs/BATCH_MODE.md`](docs/BATCH_MODE.md).
The multi-target source, project-output, import, and export philosophy is
specified in [`docs/TARGET_PROFILES.md`](docs/TARGET_PROFILES.md). The
hardware-aware sprite, composite-sprite, character-bank, pattern-reservation,
and Multicolor authoring workspace is specified in
[`docs/VDP_DESIGN_TOOLS.md`](docs/VDP_DESIGN_TOOLS.md).

## Technology

- C++20 conversion and file-format core
- Qt 6.8 or newer
- Qt Quick Controls 2 user interface
- CMake build system
- Zed-friendly C++ development through `clangd` and CMake compilation data
- CTest-driven validation and golden-file compatibility tests

## Build

Install Qt 6.8 or newer with Qt Quick, CMake, and a native C++20 toolchain.
The committed presets use Qt MinGW on Windows, GCC or Clang on Linux, and
AppleClang on macOS:

```shell
cmake --preset <platform-preset>
cmake --build --preset <platform-preset>
ctest --preset <platform-preset>
```

Choose `windows-mingw-debug`, `windows-mingw-release`, `linux-debug`, or
`macos-debug` for `<platform-preset>`. Use the Windows Release preset for
normal use and performance evaluation; Debug is intended for diagnostics and
is substantially slower in the exhaustive-search modes. The Windows presets
match the toolchain installed at
`C:\Qt` on the current development machine. Linux and macOS expect Qt, CMake,
and Ninja to be discoverable in the shell environment. Machine-specific
overrides belong in the ignored `CMakeUserPresets.json` file.

The build directory initially contains only the application executable. To
copy Qt, MinGW, plug-ins, and QML runtime files beside it so the executable can
be launched directly from Explorer, run:

```powershell
.\tools\deploy_windows_preview.ps1
```

The command above defaults to the Debug configuration; its runnable folder is
`build\windows-mingw-debug\bin`. Deployment explicitly uses the release Qt
runtime shipped by the installed MinGW kit, even though the application itself
retains Debug symbols.

To incrementally rebuild, deploy, and launch the optimized Release application
in one step, run:

```powershell
.\tools\run_windows_preview.ps1
```

The release-specific launcher is an equivalent convenience alias. To reuse a
known-current build without rebuilding, pass `-SkipBuild`:

```powershell
.\tools\run_windows_release_preview.ps1
.\tools\run_windows_preview.ps1 -SkipBuild
```

Close the preview window before rebuilding the application because Windows
locks a running executable.

The same build also creates `retrovdp-cli` in the `bin` directory. A
minimal headless conversion is:

```shell
retrovdp-cli --input artwork.png --output converted --mode bitmap-9918a --format tifiles
```

Add `--json` for machine-readable results. See
[`docs/COMMAND_LINE.md`](docs/COMMAND_LINE.md) for all modes, presets, formats,
overwrite behavior, and exit codes.

Zed users can run the matching configure, build, and test entries from the
task picker. CMake writes `compile_commands.json` into each build directory so
Zed's `clangd` language server receives the project's actual compile flags.

## Development checks

The repository includes `.editorconfig`, `.clang-format`, and `.qmlformat.ini`
so C++, QML, and basic text formatting remain consistent across editors and
operating systems. GitHub Actions configures, builds, and tests every change on
Windows with MinGW, Ubuntu with GCC, and both Intel and Apple Silicon macOS
runners with AppleClang.

## Project structure

- `app/` — Qt desktop shell, QML interface, shared application pipeline, and CLI
- `include/retrovdp/core/` — public, platform-neutral core API
- `include/retrovdp/formats/` — portable export requests and manifests
- `include/retrovdp/imageio/` — Qt image-loading adapter API
- `src/core/` — conversion and independent codec implementation
- `src/formats/` — deterministic retro output writers
- `src/imageio/` — common raster, PNG, and filesystem adapters
- `tests/` — unit, workflow, and golden-output tests
- `docs/` — architecture and migration decisions

The untouched upstream reference checkout is kept outside this repository so
original and ImgSource-related material cannot enter the new implementation.
Its exact audited commit is pinned in the behavioral-baseline document.

## Attribution

Convert9918 was created by Mike Brent (Tursi/HarmlessLion.com). RetroVDP Studio
is inspired by that work and retains the original copyright, license terms,
visible attribution, and link required by the project's governing terms.

The RetroVDP Studio cross-platform architecture, Qt interface, user
experience, and new features are created by Cisco Garcia / CiscoGarciaFL.

See [`NOTICE.md`](NOTICE.md) for full attribution and [`LICENSE`](LICENSE) for
the governing terms. This project uses the same license terms as the original
Convert9918 project with the original author's permission. Commercial use and
distribution under different terms require prior permission from the original
author.
