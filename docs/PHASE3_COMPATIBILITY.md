# Phase 3 compatibility review

Phase 3 now has portable implementations for all nine conversion modes,
focused deterministic fixtures for their table contracts, cross-platform CI,
cancellable preview-job generations, and a reproducible performance/memory
baseline.

## Compatibility status

- Bitmap 9918A, Greyscale, Black-and-White, Bitmap Color Only, Multicolor,
  Dual Multicolor, Half Multicolor, and fixed-palette F18A preserve the audited
  search constraints, palette-index remapping, and target-memory layouts.
- Scanline Palette Bitmap F18A intentionally uses an independent RGB444
  median-cut palette per row. The original uses a stateful neighborhood-weighted
  merger whose result depends on error propagated from earlier scanlines. The
  deterministic replacement is documented in successful conversion diagnostics
  and is designed for independent testing and future parallel execution.
- The approved original corpus and hashes remain in `tests/golden/reference`.
  Phase 5 now compares RAW and framed TIFILES files generated from captured
  target payloads, closing the writer-boundary check. Core tests retain
  controlled prepared-image fixtures for focused conversion failures. A full
  source-decode-through-conversion golden assertion remains separate from the
  exporter contract.

## Exit decision

The Phase 3 implementation checklist is complete, as are the Phase 4 input
adapter and Phase 5 exporters. Captured TIFILES payloads now verify framing
independently from conversion; final source-to-target parity review must still
account for the documented scanline-palette difference.
