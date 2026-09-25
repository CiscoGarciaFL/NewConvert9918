# Legacy and new application performance comparison

This comparison answers whether New Convert 9918 only feels slower because of
its interface or whether the conversion engine is measurably slower than
Convert9918 1.9.1. After the completed parity optimization pass, the new engine
is faster in all nine measured modes. Bitmap and Paletted F18A are effectively
at compute parity, while the complete New Convert total is 0.82x the legacy
total. Algorithmic innovation can now be measured separately against this
stable pre-innovation baseline.

## Method

- Host: the Windows development VM, 4 logical processors
- Latest run: 2026-09-20; pre-optimization baseline: 2026-09-19
- Input: `tests/golden/source/photo-landscape.png`, 1536x1024 RGB
- Legacy executable: Convert9918 1.9.1.0, verified SHA-256
  `7a97a25cf58adcf55e81d714a62a51d1f65ff2298bd3ea6b05348415f17f0c9a`
- New executable: parity-optimized MinGW GCC 13.1 Release build of
  `newconvert9918-cli`; historical totals are retained below
- Settings: the audited legacy defaults and matching `balanced` preset
- Output: TIFILES tables; the legacy command line additionally writes its BMP
  preview where supported
- Scope: fresh-process wall time, including input decode, transform,
  conversion, manifest generation, and output writing
- Release sampling: one cold run plus the median of three warmed runs per mode

The applications were not left running concurrently. Mode order was fixed,
but program order alternated between samples to reduce systematic thermal and
cache bias. Cold totals were within 3% of warm totals, so process startup and
filesystem caching are not the main explanation.

The repeatable harness is `tools/benchmark_side_by_side.ps1`. It verifies the
legacy executable hash, uses isolated temporary output directories, rejects
failed processes, and removes its temporary outputs after the run.

## Results

Times below are warmed wall-clock seconds from the final parity run. A ratio
below 1.0 means the new engine is faster. “Parity-pass gain” compares this run
with the immediately preceding 36.887-second first-pass result.

| Conversion mode | Legacy 1.9.1 | New Release | New / legacy | Parity-pass gain |
| --- | ---: | ---: | ---: | ---: |
| Bitmap 9918A | 6.879 | 6.814 | 0.99x | 30.0% |
| Greyscale Bitmap 9918A | 3.619 | 3.350 | 0.93x | 36.1% |
| Black-and-White Bitmap 9918A | 0.568 | 0.381 | 0.67x | 5.2% |
| Multicolor 9918 | 0.396 | 0.259 | 0.65x | 0.4% |
| Dual Multicolor 9918 | 0.576 | 0.272 | 0.47x | 18.5% |
| Half Multicolor 9918A | 6.485 | 5.432 | 0.84x | 31.0% |
| Bitmap Color Only 9918A | 0.449 | 0.333 | 0.74x | 6.1% |
| Paletted Bitmap F18A | 4.854 | 4.645 | 0.96x | 24.1% |
| Scanline Palette Bitmap F18A | 7.770 | 4.385 | 0.56x | 33.3% |
| **All modes** | **31.595** | **25.872** | **0.82x** | **29.9%** |

The measured progression is intentionally separated into reproducible stages:

- Initial Release: 54.028 seconds versus 34.015 legacy, or 1.59x.
- First optimization pass: 36.887 seconds versus 31.111 legacy, or 1.19x.
- Completed parity pass: 25.872 seconds versus 31.595 legacy, or 0.82x.

The final new total is 52.1% below the initial new baseline. Bitmap and
Paletted F18A are within 4% of legacy, which is the desired compute-parity
state; the other seven modes are measurably faster without changing their
search policy or output bytes.

CPU time closely tracks wall time in both Release applications. The expensive
work is CPU-bound and largely single-threaded; Qt window rendering, file I/O,
and process startup do not account for the multi-second gap.

Median peak working set ranged from 15.2 to 15.7 MiB for the legacy process and
29.4 to 30.6 MiB for the new CLI. The new process therefore uses roughly 14 MiB
more resident memory during these runs, but CPU time—not memory pressure or
allocation size—remains the binding performance constraint on this VM.

## Why the expensive modes differ

Bitmap, Half Multicolor, and Paletted F18A perform an exhaustive search for
the best foreground/background pair and 8-bit pattern for every eight-pixel
row. At fifteen colors, one block may examine 105 unordered color pairs, 256
patterns, and up to eight pixels per pattern. Early rejection helps, but this
is still the dominant cost.

The original implementation precomputes the palette side of its YCrCb color
space and compiles specialized quantizers for perceptual/YCrCb and
error-distribution/ordered variants. New Convert now follows the same
structure: fixed palette and mixed colors are prepared once, hot formulas are
inlined in specialized paths, and Release builds use whole-program optimization
plus fast floating point only in the quantizer translation units.

Dual Multicolor and Half Multicolor cache their mixed-color candidates,
luminance limits, and shifted source blocks. The fast floating-point scope is
deliberately narrow so settings validation retains strict NaN and infinity
handling. These changes preserve candidate enumeration and tie-breaking, and
the complete golden suite remains the byte-level safety gate.

Scanline Palette F18A is faster in the new engine, but it intentionally uses
the deterministic palette-selection policy documented in the Phase 6A review,
not the original stateful neighborhood merger. Its timing is therefore useful
for user experience but not evidence that identical algorithms were optimized.

## Why the interactive preview can feel worse

`tools/run_windows_preview.ps1` now rebuilds and launches the Release executable
by default, matching the optimized build used for the measurements above. Pass
`-Configuration Debug` when development diagnostics are required, or
`-SkipBuild` only when intentionally reusing an already-current build. The
measured Debug engine is roughly four times slower than Release on the
search-heavy modes.

The new interface also converts automatically after a 160 ms debounce, while
the original normally waited for an explicit Reload. Generation IDs prevent an
older result from replacing a newer preview. The mode converters now observe
cancellation at scanline or block-row boundaries, so rapid adjustments stop
obsolete work promptly instead of leaving superseded searches consuming CPU.

## Implemented immediate optimizations

1. The Release preview launcher makes subjective checks representative of an
   optimized build.
2. Long-running converters stop at row or block boundaries when superseded.
3. Fixed palette and mixed colors retain prepared YCrCb values rather than
   repeating their transforms inside the pattern search.
4. Multicolor modes cache shifted source blocks, mixed colors, and luminance
   values that do not change during candidate enumeration.
5. Hot quantizers compile separate perceptual/YCrCb and error/no-error variants,
   matching the legacy macro-generated structure without inner-loop branches.
6. Release builds use supported interprocedural optimization, matching the
   legacy whole-program optimization setting.
7. Fast floating point is restricted to the quantizer implementation files;
   strict validation and all other core code retain normal IEEE behavior.
8. The complete parity benchmark and golden suite pass. Further conversion
   work should be treated as a new algorithmic-improvement phase and measured
   against this result rather than folded into parity work.

Performance changes must retain deterministic tables and pass the complete
golden and interface suites. The goal is to remove redundant computation and
obsolete work, not to substitute faster approximations silently.
