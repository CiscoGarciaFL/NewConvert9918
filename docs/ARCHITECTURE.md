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

### Export codecs

Exporters will consume a completed conversion result and produce RAW, RLE,
TIFILES, V9T9, MSX SC2, Coleco/Adam, Extended BASIC, ROM, and preview-image
outputs as applicable.

