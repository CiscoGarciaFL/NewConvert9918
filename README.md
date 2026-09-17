# New Convert 9918

New Convert 9918 is a cross-platform reimplementation and modernization of
[Convert9918](https://github.com/tursilion/convert9918), an image conversion
tool for TMS9918A bitmap modes and F18A enhanced bitmap modes.

The project is starting with two goals:

1. Preserve the excellent output and format support of the original tool.
2. Provide a clearer, responsive interface on Windows, Linux, and macOS.

## Status

The portable core includes image geometry, preprocessing, color matching,
palette selection, dithering, all four Bitmap 9918A variants, all three
Multicolor 9918/9918A variants, and both F18A bitmap palette modes. The Phase 4
input pipeline now loads common Qt raster formats, PCX, and the original retro
formats with explicit safety limits. The Qt shell can open, drop, paste, and
display those sources. Connecting loaded images to live background conversion
remains a later interface-integration milestone.

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
Image formats, color/alpha/orientation policy, and input limits are documented
in [`docs/IMAGE_INPUT.md`](docs/IMAGE_INPUT.md).

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

Choose `windows-mingw-debug`, `linux-debug`, or `macos-debug` for
`<platform-preset>`. The Windows preset matches the toolchain installed at
`C:\Qt` on the current development machine. Linux and macOS expect Qt, CMake,
and Ninja to be discoverable in the shell environment. Machine-specific
overrides belong in the ignored `CMakeUserPresets.json` file.

After building on the current Windows development machine, launch the visible
Qt shell from PowerShell with:

```powershell
.\tools\run_windows_preview.ps1
```

Close the preview window before rebuilding the application because Windows
locks a running executable.

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

- `app/` — Qt application shell and QML interface
- `include/newconvert9918/core/` — public, platform-neutral core API
- `include/newconvert9918/imageio/` — Qt image-loading adapter API
- `src/core/` — conversion and independent codec implementation
- `src/imageio/` — common raster and file-routing implementation
- `tests/` — unit and future golden-output tests
- `docs/` — architecture and migration decisions

The untouched upstream reference checkout is kept outside this repository so
original and ImgSource-related material cannot enter the new implementation.
Its exact audited commit is pinned in the behavioral-baseline document.

## Attribution

Convert9918 was created by Mike Brent (Tursi/HarmlessLion.com). New Convert
9918 retains his original copyright, license terms, visible attribution, and
a link to the original project.

The New Convert 9918 cross-platform architecture, Qt interface, user
experience, and new features are created by Cisco Garcia / CiscoGarciaFL.

See [`NOTICE.md`](NOTICE.md) for full attribution and [`LICENSE`](LICENSE) for
the governing terms. This project uses the same license terms as the original
Convert9918 project with the original author's permission. Commercial use and
distribution under different terms require prior permission from the original
author.
