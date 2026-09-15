# Migration plan

## Phase 0 — Rights and provenance

- Use the original Convert9918 license terms with the original author's
  permission.
- Preserve full attribution for Mike Brent/Tursi and separately licensed
  components.
- Credit Cisco Garcia/CiscoGarciaFL for the new cross-platform architecture,
  Qt interface, user experience, and features.
- Do not reuse the retired ImgSource library or its implementation.

## Phase 1 — Behavioral baseline

- Select representative source images, including awkward aspect ratios and
  palette edge cases.
- Generate reference previews and binary exports with the original Windows
  application.
- Catalog every conversion mode, option, and allowed export combination.

## Phase 2 — Portable core

- Introduce image-buffer, settings, result, palette, and diagnostics types.
- Port one mode at a time, beginning with TMS9918A Graphics II bitmap mode.
- Compare every result byte-for-byte with the reference corpus.

## Phase 3 — Image and retro codecs

- Use Qt image I/O for common formats.
- Implement independently testable codecs for unsupported and retro formats.
- Add malformed-input and size-limit tests.

## Phase 4 — Product workflow

- Open or drop an image.
- Choose fit/crop and positioning with a live source preview.
- Select a target mode and tune conversion settings.
- Compare original and converted output without a manual Reload step.
- Export with a clear summary of generated files.

## Phase 5 — Delivery

- Build and test with MSVC on Windows, GCC/Clang on Linux, and Apple Clang on
  macOS.
- Package shared Qt libraries and required image-format plugins.
- Publish signed release artifacts after platform smoke testing.
