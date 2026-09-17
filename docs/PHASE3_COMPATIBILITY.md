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
  Exact end-to-end byte comparison is not asserted inside the core suite yet.
  Phase 4 now supplies the source decoding/scaling boundary; Phase 5 must add
  TIFILES framing and connect the complete golden pipeline. Core tests currently
  validate raw target-table bytes with controlled prepared-image fixtures.

## Exit decision

The Phase 3 implementation checklist is complete, as is the Phase 4 image-input
adapter. Its final golden-output gate remains open until Phase 5 exporters feed
the captured corpus through the complete pipeline. At that point each raw
target payload can be compared with the captured TIFILES payload, while the
scanline-palette mode is evaluated against the intentional difference above.
This dependency is kept explicit rather than treating partial-pipeline hashes
as proof of end-to-end parity.
