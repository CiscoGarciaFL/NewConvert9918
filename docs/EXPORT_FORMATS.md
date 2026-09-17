# Export formats

Phase 5 separates deterministic file construction from Qt filesystem and PNG
services. The standard-C++ `newconvert9918_formats` library accepts a completed
`TargetMemoryImage` and returns a `GeneratedFileManifest`; it never opens a
file or prompts the user. The Qt `newconvert9918_imageio` adapter encodes PNG,
preflights the complete manifest for conflicts, and writes each approved file
with `QSaveFile`.

## Common contract

`ExportRequest` identifies one format, a single filename stem, completed target
tables or a preview as required, and optional legacy loader templates. A stem
must be one filename rather than a path, preventing an export from escaping
the directory selected by the caller. A successful manifest contains every
filename and byte vector before any filesystem mutation occurs.

`writeExportManifest()` checks all destination names first. If any file exists,
it returns `WouldOverwrite` and the complete conflict list without writing
anything. The application must present that list and call again with explicit
overwrite approval. Writes use same-directory temporary files and atomic
replacement where the platform supports it.

## Formats and applicability

| Format | Output contract | Conversion modes |
| --- | --- | --- |
| RAW | One uppercase `.TIAP`, `.TIAC`, or `.TIAM` file per target table | All modes |
| RLE | Same names; original 127-byte packet encoding and paired bitmap normalization; palette tables stay raw | All modes |
| TIFILES | RAW payload with a 128-byte PROGRAM header | All modes |
| V9T9 | RAW payload with a 128-byte V9T9 header and a six-character base name | All modes |
| MSX Screen 2 | Header, 6 KiB pattern, name table, disabled sprites, 6 KiB color | Bitmap, greyscale, black-and-white, color-only |
| Coleco CVPaint | 6 KiB pattern followed by 6 KiB color | Bitmap, greyscale, black-and-white, color-only |
| Adam PowerPaint | 240×160 pattern then color, padded to 10 KiB | Bitmap, greyscale, black-and-white, color-only |
| Adam HGR/HGRH | 21-byte header, 5 KiB color, 5 KiB pattern | Bitmap, greyscale, black-and-white, color-only |
| ColecoVision ROM | Validated loader, patched table pointers, pattern and color payloads | Bitmap, greyscale, black-and-white, color-only |
| Extended BASIC | Validated TIFILES loader with direct or RLE tables and relocated program records | Bitmap, greyscale, black-and-white, color-only |
| PNG | Lossless RGB/RGBA preview encoded by `QImageWriter` | All modes |

Black-and-white exports that require a color table synthesize the established
black-on-white `0x1F` table. The broken original ColecoVision RLE cartridge
choice remains deliberately unsupported, as recorded in
`docs/ORIGINAL_DEFECTS.md`.

## Loader templates and provenance

ColecoVision ROM and Extended BASIC files contain executable target-machine
loaders in addition to converted image data. Their writers therefore accept a
caller-supplied template, validate all patch locations and size limits, and
fail clearly when no suitable template is available. The production library
does not embed or copy the original application's opaque binary resources.

Tests use only the already-approved behavioral captures as injected fixtures.
They prove the pointer, record, relocation, and image-data algorithms while
preserving the repository decision that the pinned upstream checkout and its
resources remain outside the implementation. A future distributor can supply
an independently built or separately authorized loader without changing the
portable writer.

## Compatibility verification

The export tests compare RAW, RLE, TIFILES, V9T9, MSX SC2, CVPaint,
PowerPaint, HGR, ColecoVision ROM, and direct Extended BASIC output with the
pinned Convert9918 1.9.1 captures. The XB RLE test verifies the original record
count, addresses, compressed data, and relocation outside template-owned
bytes. Separate tests exercise every target-table layout, unsafe filenames,
mode rejection, PNG RGB/alpha round trips, complete overwrite preflight, and
explicit replacement.

These tests run in the existing Windows, Ubuntu, Intel macOS, and Apple
Silicon macOS CI matrix, so filenames and byte layouts are checked on all
supported host platforms.
