# Phase 6A parity review

Status: historical verification record. This review is complete and is not an
active implementation plan. Current work is tracked in
[../PROJECT_PLAN.md](../PROJECT_PLAN.md).

Phase 6A closes the user-facing legacy-feature audit. A checked item means
either that the behavior is available and tested or that an intentional
difference has been recorded here. It does not mean that unverified legacy
machine code or stateful defects were copied into the clean implementation.

## Implemented controls

- Ordered dithering uses the original 0–16 darkening range. Increasing the
  value subtracts more from the threshold map and therefore darkens ordinary
  input samples. Black and white bypass ordered adjustment, matching the
  established compatibility policy.
- Perceptual red, green, and blue weights are editable as percentages,
  persisted, undoable, and restorable to the audited 30/52/18 defaults.
- Maximum color shift exposes the complete 0–100% core range.
- The fifteen-color TMS9918A working palette is editable with the native Qt
  color dialog, persistent, undoable, and restorable. It is also the palette
  offered to the source-background picker.
- Paletted F18A conversion exposes deterministic RGB444 Median Cut and
  horizontally weighted Popularity selection.
- Scanline F18A conversion supports 0–14 shared colors, Median Cut or
  Popularity selection for those colors, and independent inclusion of bitmap
  regions 1, 2, and 3. Shared colors occupy stable palette slots on all 192
  rows. Per-line dynamic colors continue to use deterministic RGB444 Median
  Cut rather than the original stateful neighborhood merger.
- PowerPaint framing scales into a 240×160 active area at the upper left and
  pads the remaining 256×192 target with opaque black, as the audited program
  does before conversion.

All settings above participate in reset, persistence, undo, manual/automatic
update, validation, and the interface workflow tests.

## Intentional interaction differences

- Source nudging remains one pixel per click and may traverse the available
  target/crop range. The original seven-pixel horizontal cap and hidden
  Shift/Ctrl acceleration are not reproduced. The visible controls are
  predictable across mouse, touch, keyboard, Windows, Linux, and macOS; larger
  movement is available through repeated activation without modifier-only
  behavior.
- Random-folder slideshow, recursive indexing, clipboard polling, and
  cross-instance shared-memory filename synchronization are deferred beyond
  v1. They are secondary Windows workflows, not conversion requirements.

## Compatibility-gated exclusions

- **Toon matching:** not exposed in v1. The source audit identifies a
  palette-order-specific restricted hue classifier, but the approved golden
  corpus does not include executable captures with Toon enabled. Recreating
  it before independent input/output fixtures exist would claim compatibility
  that has not been measured.
- **Executable loader exports:** ColecoVision ROM, Extended BASIC, and
  Extended BASIC RLE writers remain available at the portable library
  boundary but are not offered by the application. They require independently
  built or separately authorized target-machine loader templates. No original
  embedded loader bytes are copied or redistributed.
- **Byte-for-byte legacy equivalence:** the hash-pinned Convert9918 1.9.1
  capture corpus is approved as the behavioral oracle. Current automated
  tests approve deterministic RetroVDP Studio output, file layouts, table
  validation, and the documented compatibility cases; they do not assert that
  every table is byte-identical to the stateful original quantizers. Known
  algorithm differences include Qt-based scaling/histogram handling and the
  deterministic scanline palette merger described above.

The original broken ColecoVision RLE cartridge writer and the undispatched
`.jpc` reader remain excluded as documented defects.

## Exit decision

Phase 6A is complete. Every approved legacy conversion control is exposed and
tested, while every control still gated by provenance or missing executable
evidence has an explicit policy above. Future compatibility work may revise a
decision only by adding independently captured fixtures and updating this
review.
