# Architecture

## Design rules

1. Conversion behavior is independent of the user interface.
2. The core uses standard C++ value types and does not expose native Windows,
   macOS, Linux, or QML types.
3. Qt adapts files, clipboard data, and display images at the application edge.
4. Retro file formats live in independent codecs with documented inputs and
   outputs.
5. Every supported conversion mode is covered by deterministic fixtures.

## Layers

### Application

The Qt Quick application owns workflow, drag-and-drop, file dialogs,
preferences, background jobs, and preview presentation.

### Image I/O

Qt's image readers and writers handle common image formats. Dedicated codecs
will handle PCX and machine-specific formats where Qt has no built-in support.

### Conversion core

The core will own scaling policy, palette selection, color matching,
dithering, TMS9918A constraints, F18A extensions, and conversion diagnostics.
It must be callable from the GUI, tests, and a future command-line frontend.

The portable `RgbImage` boundary uses interleaved 8-bit RGB or RGBA channels.
Its row stride is explicit and includes any padding at the end of each stored
row. A valid buffer therefore contains exactly `rowStride × height` bytes,
while `width × bytesPerPixel` is the minimum permitted stride. Layout
validation checks every multiplication before allocation. Default limits are
16,384 pixels per dimension, 64 Mi pixels, and 256 MiB of stored bytes; callers
may supply tighter limits at trust boundaries.

`ConversionRequest` holds immutable shared ownership of a source image so GUI
preview jobs can reuse decoded pixels without large copies. `ConversionResult`
keeps completion, failure, and cancellation distinct, and carries an optional
preview plus structured diagnostics with stable machine-readable codes.
Warnings do not make an otherwise successful result fail; error diagnostics
do. Target palettes and memory tables are added independently as their types
are defined.

Target palettes contain one to sixteen 24-bit RGB colors. Encoded target
memory is stored as role-tagged byte tables rather than host structs, avoiding
padding and byte-order ambiguity. Each conversion mode publishes one exact
ordered table layout derived from the approved original captures; validation
checks table count, role, and byte size before data reaches an exporter.

Image geometry is a separate core operation. Fit mode preserves the complete
source and centers any letterbox area; start, center, and end fill modes scale
to cover the target and crop along the overflowing axis. Crop offsets are
expressed in scaled-source pixels and are clamped to the valid region. The
Box, Gaussian, Hamming, Blackman, and Bilinear filters use deterministic
separable resampling, while None preserves source pixels without resampling.
All paths produce an exact target-sized RGB/RGBA image and validate both the
intermediate and output allocations before processing.

Color matching preserves the original converter's two distance metrics. The
default path converts RGB samples to unoffset YCrCb using the original matrix,
multiplies the luminance difference by the configurable emphasis, and then
sums squared component differences. Perceptual matching instead sums squared
RGB differences with configurable channel weights. These routines accept
double-precision, temporarily out-of-range RGB samples so error diffusion does
not need to clamp before measuring a candidate palette color.

Image preprocessing is also isolated from quantization. Histogram stretching
uses a deterministic global brightness CDF mapped to the 32-224 output range
passed by the original program to ImgSource. A shared delta is applied to the
three RGB channels to retain chroma unless a channel reaches a limit. Gamma
correction then applies `pow(channel / 255, 1 / gamma) * 255`, preserving the
original operation order and truncation. RGBA alpha and row padding are never
adjusted. The ImgSource equalizer's private implementation is unavailable, so
its exact histogram parity remains a golden-output question; the portable
algorithm is specified and tested independently rather than importing that
abandoned binary dependency.

Palette selection exposes deterministic median-cut and popularity paths. The
F18A-compatible median cut reduces channels to four bits before partitioning,
splits the block with the longest RGB range at `(count + 1) / 2`, averages with
integer truncation, and expands each result nibble to eight bits. An RGB888
variant is available for future non-target previews. Range and channel ties
use stable creation and RGB ordering instead of depending on a standard
library heap implementation. Popularity selection counts RGB444 colors,
optionally applies the original eight-band horizontal center weighting, merges
neighboring colors among the leading candidates, and ranks equal counts by
ascending packed RGB value.

Dithering is represented by six-cell error-distribution kernels whose weights
are sixteenths: down-left, down, down-right, right, two pixels right, and two
rows down. The portable error buffer clips contributions at image edges and
preserves the original Average behavior of dividing stored error by three on
all rows after the first; Accumulate applies it directly. Ordered dithering
uses the original x-first 2x2 and 4x4 threshold maps, subtracts the configured
brightness in sixteenths, and scales each RGB channel by that threshold.
Near-black and near-white samples bypass ordered adjustment, matching the
original converter. Samples remain double precision and unclamped until color
matching.

The Bitmap 9918A converter is the first complete target mode. It consumes an
already scaled and preprocessed 256x192 RGB/RGBA image plus the original
fifteen-color working palette. Each eight-pixel scanline block is searched for
the best foreground, background, and pattern combination while preserving
candidate-dependent horizontal error. Selected errors then flow into the
bounded diffusion buffer. The result contains a rendered RGB preview and
validated 6 KiB pattern and color tables in Graphics II memory order. Working
palette indexes are remapped to hardware color codes only when encoding the
color table.

### Export codecs

Exporters will consume a completed conversion result and produce RAW, RLE,
TIFILES, V9T9, MSX SC2, Coleco/Adam, Extended BASIC, ROM, and preview-image
outputs as applicable.
