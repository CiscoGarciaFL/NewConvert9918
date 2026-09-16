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

### Export codecs

Exporters will consume a completed conversion result and produce RAW, RLE,
TIFILES, V9T9, MSX SC2, Coleco/Adam, Extended BASIC, ROM, and preview-image
outputs as applicable.
