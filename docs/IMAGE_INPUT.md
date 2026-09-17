# Image input policy and format support

Phase 4 replaces ImgSource and the original Windows-only input paths with a
bounded, cross-platform image-loading pipeline. Every successful reader emits
the same platform-neutral `RgbImage` representation used by the conversion
core.

## Common raster formats

`QImageReader` handles PNG, JPEG, BMP, and GIF. TIFF and WebP use the same path
when the corresponding Qt Image Formats plug-in is installed. The application
reports a normal unsupported-format error when an optional plug-in is absent.

The raster policy is deterministic:

- embedded orientation is applied before pixels enter the core;
- a valid embedded color profile is converted to sRGB;
- an untagged image is treated as sRGB without changing its channel values;
- source alpha is preserved as straight RGBA8888; opaque sources use RGB888;
- only the first GIF or other animated-image frame is loaded, while frame
  count and animation status remain available as metadata.

## PCX

The in-tree PCX reader is a clean C++20 implementation with no external code
or runtime dependency. It accepts uncompressed and PCX-RLE data in these
well-defined variants:

- 1-bit images with one to four color planes and the 16-color header palette;
- 8-bit indexed images with the required trailing 256-color palette;
- 24-bit RGB and 32-bit RGBA planar images.

Invalid geometry, unsupported plane arrangements, overflowing scanline sizes,
RLE runs that exceed the decoded buffer, missing palettes, and truncated data
are rejected before an image is returned.

## Retro formats

The standard-C++ retro readers reconstruct 256×192 RGB images and are tested
without Qt or operating-system APIs:

| Format | Accepted layout |
| --- | --- |
| TI Artist | `.TIAP`/`.TIAC`/`.TIAM` and `_P`/`_C`/`_M` companion sets; raw or RLE tables with raw, TIFILES, or V9T9 framing |
| MSX Screen 2 | Seven-byte prefix, 6 KiB pattern table at `0x0007`, 6 KiB color table at `0x2007` |
| Coleco CVPaint | 6 KiB pattern followed by 6 KiB color data |
| Adam PowerPaint | 5 KiB pattern followed by 5 KiB color data, padded to the Graphics II canvas |
| Adam HGR/HGRH | 21-byte header, 5 KiB color data, then 5 KiB pattern data |

TI Artist loads a pattern companion first, uses the matching color companion
when present, and otherwise applies the original monochrome default. A 32-byte
or 6 KiB `_M`/`.TIAM` companion supplies F18A global or per-scanline RGB444
palettes. A 2 KiB companion supplies the Half Multicolor underlay, which is
averaged with the Graphics II overlay just like its two-frame preview.

## Trust boundaries

The default encoded-file limit is 64 MiB. Decoded images retain the core's
limits of 16,384 pixels per dimension, 64 Mi pixels, and 256 MiB. Callers can
supply tighter values. Qt-declared dimensions are checked before decoding when
available and are checked again after decoding; every independent reader
checks arithmetic and exact table requirements before allocation or access.

Malformed, truncated, unsupported, over-limit, and missing-companion cases
produce errors rather than partial images. The test suite covers those paths
alongside exact pixel checks for each retro layout and both PCX encodings.

## Interactive input

The Qt application routes file-open selection, a command-line source path,
file drag-and-drop, and clipboard images through the same loader. Clipboard
input uses Qt's cross-platform image MIME abstraction. Remote URLs are not
loaded; drag-and-drop accepts local files only.
