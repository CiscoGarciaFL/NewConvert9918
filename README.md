# New Convert 9918

New Convert 9918 is a cross-platform reimplementation and modernization of
[Convert9918](https://github.com/tursilion/convert9918), an image conversion
tool for TMS9918A bitmap modes and F18A enhanced bitmap modes.

The project is starting with two goals:

1. Preserve the excellent output and format support of the original tool.
2. Provide a clearer, responsive interface on Windows, Linux, and macOS.

## Status

The project is in its initial architecture and parity-testing phase. The Qt
shell currently contains only the proposed application workflow; conversion
algorithms and file-format codecs have not yet been ported.

See [`PROJECT_PLAN.md`](PROJECT_PLAN.md) for the durable roadmap, current
status, acceptance criteria, and next-session checklist.

## Technology

- C++20 conversion and file-format core
- Qt 6.8 or newer
- Qt Quick Controls 2 user interface
- CMake build system
- Qt Test and golden-file compatibility tests

## Build

Install Qt 6.8 or newer with Qt Quick and a supported C++ toolchain, then run:

```shell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Project structure

- `app/` — Qt application shell and QML interface
- `include/newconvert9918/core/` — public, platform-neutral core API
- `src/core/` — conversion-core implementation
- `tests/` — unit and future golden-output tests
- `docs/` — architecture and migration decisions

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
