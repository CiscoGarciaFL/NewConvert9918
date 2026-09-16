# Original Convert9918 defects and compatibility policy

This document separates defects in the pinned Convert9918 1.9.1 reference from
behavior that New Convert 9918 intends to reproduce. A defect is listed as
confirmed only when the source and an executable observation agree, or when
the original interface labels the behavior as broken.

## Confirmed defects

### Dots in output directory names truncate the save path

The original save routine searches the entire output path for the final dot
and removes everything after it before adding the requested extension. A dot
in a parent directory is therefore treated as a filename extension.

Observed on 2026-09-15: capturing to a directory named `original-1.9.1`
placed the TIFILES outputs at `reference/ORIGINAL-1.9.TIAP` and
`reference/ORIGINAL-1.9.TIAC`, rather than beside the requested output base.
The source performs this operation with `outfile.ReverseFind('.')` before it
constructs the final filename.

New Convert 9918 must interpret extensions only on the final path component.
This is an intentional safety fix, not behavior to reproduce. The reference
capture uses `original-1_9_1` so the original executable can save correctly.

### ColecoVision RLE cartridge output is marked broken

The original save dialog names this choice `ColecoVision RLE Cart (Broken)`.
Until a compatible consumer and byte-layout test prove otherwise, New Convert
9918 must not present this format as a reliable export. If implemented for
forensic compatibility, it must remain explicitly marked experimental.

The controlled 2026-09-15 export capture therefore omitted this writer while
capturing every other non-TIFILES Save Pic choice. The exclusion is recorded
in `tests/golden/reference/original-1_9_1/export-captures.json` so the missing
file cannot be mistaken for an incomplete capture.

## Unconfirmed compatibility questions

- The save code questions whether RLE TIFILES headers contain the wrong file
  size.
- `.jpc` files are indexed by slideshow mode but have no matching reader in
  the audited dispatch.
- Documentation and initialized state disagree about Average versus
  Accumulate as the default error mode.
- Documentation describes older 30/59/11 perceptual weights, while version
  1.9.1 initializes and resets to 30/52/18.
- GIF comments acknowledge incomplete animated-transparency behavior.
- TI Artist raw/RLE interpretation depends on Shift/Ctrl/Alt state at load
  time and is not self-describing.
- Per-scanline F18A PNG export is rejected.
- Save validity depends on a manual Reload and can refer to the prior mode.

These items need focused executable tests before being promoted to confirmed
defects. Byte-level compatibility should be preserved only where it benefits
existing files or tools; unsafe path handling, misleading status, and stale UI
state should be corrected and covered by regression tests.
