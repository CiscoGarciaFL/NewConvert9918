# Conversion Acceleration Research

Date: 2026-09-20

Status: research and implementation-planning document; no accelerator is
implemented by this document.

## Purpose

This study evaluates four ways to reduce conversion latency and increase batch
throughput without prematurely changing Convert9918-compatible output:

1. wider CPU arithmetic and SIMD;
2. improved algorithms, caches, and lookup tables;
3. cooperative multicore processing; and
4. optional GPU compute with a portable CPU fallback.

All speed factors are engineering estimates, not commitments. They are anchored
to the reproducible Windows MinGW Release measurements in
`PERFORMANCE_COMPARISON.md`. Each proposal must be benchmarked on representative
photographs, pixel art, flat-color images, and future animation frames before it
is accepted.

## Output-change classification

The labels below are used throughout this document. They describe the intended
relationship to the current byte-level compatibility output, not merely whether
the image looks similar on screen.

- **NON-ALTERING — parity-safe:** The method can remain in the normal
  compatibility path if it preserves operation order, candidate order, tie
  breaking, rounding, and all relevant state. It still requires differential
  tests against the scalar reference.
- **POSSIBLY ALTERING — conditional:** The method can be parity-safe under a
  carefully specified implementation, but a different lane reduction, numeric
  precision, cache key, compiler transformation, or parallel schedule could
  select different pattern/color bytes. It needs adversarial comparisons and a
  documented fallback before becoming the default.
- **ALTERING — intentional new output contract:** The method changes the
  search, precision, or quality tradeoff in a way that can select different
  output. It belongs behind an explicit fast/quality option or a separately
  named backend, not silently in the legacy-compatible path.

### Classification at a glance

| Method | Classification | Compatibility condition |
| --- | --- | --- |
| Do not widen stored 8-bit pixels/indexes | **NON-ALTERING** | No change; this is a rejection of an unhelpful optimization. |
| Runtime ISA dispatch with validated scalar-equivalent kernels | **NON-ALTERING** | Each target must pass byte-level differential tests. |
| Explicit SIMD `double` | **POSSIBLY ALTERING** | Preserve scalar candidate/reduction order; retain scalar fallback for mismatches. |
| Shared-prefix search | **NON-ALTERING** | Same traversal order, arithmetic order, pruning proof, and ties. |
| Direct no-error solver | **NON-ALTERING** | Only for the no-error contract, with identical per-pixel tie rules. |
| Exact palette/table precomputation | **NON-ALTERING** | Complete settings key and identical arithmetic/results. |
| Weighted histogram palette selection | **POSSIBLY ALTERING** | Match legacy median/tie semantics; otherwise treat as a new palette mode. |
| Exact content memoization | **NON-ALTERING** | Cache key includes every state that affects the result, including incoming error. |
| Candidate-level deterministic threading | **NON-ALTERING** | Deterministic reduction and original candidate order. |
| Wavefront/tiled diffusion | **POSSIBLY ALTERING** | Boundary and reduction order must be proven equivalent. |
| `float32` SIMD/GPU kernel | **ALTERING** | Near ties and rounding can select different bytes. |
| Fixed-point kernel before parity proof | **POSSIBLY ALTERING** | Becomes parity-safe only after numeric-range and exhaustive differential validation. |
| Approximate shortlist/beam/heuristic search | **ALTERING** | It intentionally accepts a different search result. |
| GPU integer kernel after a shared fixed-point contract | **POSSIBLY ALTERING** | Exact only if it matches the approved scalar integer oracle. |

Speed numbers in later sections apply regardless of classification; a speed
factor is not evidence that output is unchanged.

## Executive conclusion

The present converter is not fundamentally an 8-bit computation. Source and
result pixels are stored as compact 8-bit channels and palette indexes, but the
hot color-distance and error-diffusion calculations use three-channel 64-bit
floating-point samples. Replacing those bytes with 32- or 64-bit storage would
increase memory traffic and would not make the conversion faster.

The strongest low-risk CPU path is instead:

1. reduce the number of candidate pixel steps with an exact shared-prefix
   search, plus a direct solution when error diffusion is disabled;
2. reorganize candidate data into SIMD-friendly arrays and use runtime-selected
   x86 and Arm vector kernels while retaining a scalar reference;
3. parallelize only independent work and future batch frames; and
4. investigate fixed-point arithmetic as a separately validated bridge to a
   portable GPU implementation.

This order matters. A GPU port of the current exhaustive `double` algorithm
would carry the current excess work into a backend where portable 64-bit
floating point is unavailable or inefficient. Improving and numerically
specifying the CPU kernel first gives both CPU and GPU implementations a shared,
testable contract.

The likely modeling target for the exact CPU work is approximately **3.4x** for
the complete nine-mode benchmark, reducing 25.872 seconds to about 7.6 seconds.
That is an implementation target, not a measured result. A mature GPU backend
may eventually reach roughly **2.5x-8x** for that complete benchmark, with much
larger gains on independent no-error and batch workloads.

## Measured baseline

The comparison below is the warm wall-clock Release baseline recorded on
2026-09-20. The VM exposed four logical processors and an Intel Xeon E5-2690.

| Conversion mode | New Release baseline (s) |
| --- | ---: |
| Bitmap 9918A | 6.814 |
| Greyscale Bitmap 9918A | 3.350 |
| Black-and-White Bitmap 9918A | 0.381 |
| Multicolor 9918 | 0.259 |
| Dual Multicolor 9918 | 0.272 |
| Half Multicolor 9918A | 5.432 |
| Bitmap Color Only 9918A | 0.333 |
| Paletted Bitmap F18A | 4.645 |
| Scanline Palette Bitmap F18A | 4.385 |
| **Total** | **25.872** |

The corresponding legacy total is 31.595 seconds. The new Release converter is
already 1.22x as fast overall, or 0.82 times the legacy elapsed time. Future
results must continue to use Release builds; Debug timings are not comparable.

## What the current conversion actually computes

The expensive bitmap-family search considers as many as 105 unordered color
pairs. For each pair it can test 256 eight-bit patterns, and each pattern can
evaluate up to eight pixels. Existing early exits reduce that upper bound, but
the remaining candidate search dominates Bitmap, Greyscale, Half Multicolor,
Paletted F18A, and Scanline Palette F18A.

The current core already includes several important optimizations:

- palette entries are prepared before the hot search;
- Dual and Half Multicolor cache their 256 possible mixed colors;
- template specialization removes perceptual/YCrCb and error/no-error branches
  from inner loops;
- quantizer sources use `-O3`, scoped fast floating-point optimization, and
  whole-program optimization; and
- gamma and histogram operations already use compact runtime mappings.

The Windows compiler currently targets the older x86-64 SSE baseline rather
than AVX, AVX2, or FMA. The present E5-2690 host supports AVX but not AVX2, so
runtime dispatch could use more of this host without making a binary that fails
on older processors. A distributable build must not globally use
`-march=native` because the generated executable also needs to run elsewhere.

### The central dependency constraint

With Atkinson or another error-distribution kernel enabled, the next source
position receives error from positions to its left and from earlier rows. The
candidate search also carries candidate-dependent horizontal error. Therefore,
the selected block sequence for the default conversion is intentionally ordered
left-to-right and top-to-bottom.

This constraint does not make acceleration impossible, but it changes where
parallelism is safe:

- candidates within the current block can be evaluated cooperatively;
- work that does not distribute error can be divided by row or block;
- Multicolor and Dual Multicolor blocks are independent;
- palette selection and conversion are separate stages in some F18A modes; and
- separate images or animation frames are ideal throughput-level jobs.

## 1. CPU arithmetic width and SIMD

### Storage width is not compute width — **NON-ALTERING (do not pursue widening)**

The 8-bit types in the core are appropriate for RGB input, palette indexes, and
target tables. Widening these stored values would not automatically perform
multiple conversions in one CPU operation. It would normally consume more
cache bandwidth.

The hot `RgbSample` arithmetic is currently `double`: 64-bit floating point per
channel. The useful form of wider processing is SIMD, where one instruction
evaluates several independent candidates. On x86, AVX can process four doubles
or eight floats in a 256-bit vector. Typical Arm NEON vectors process two
doubles or four floats in 128 bits. Exact lane counts vary by instruction set,
but the important change is a structure-of-arrays candidate layout that keeps
all lanes doing the same operation.

### Recommended CPU representations — **mixed classifications**

| Representation | Compatibility | Advantages | Risks | Recommendation |
| --- | --- | --- | --- | --- |
| Scalar `double` | **NON-ALTERING** | Most direct parity oracle | Lowest throughput | Retain permanently for verification and fallback |
| SIMD `double` | **POSSIBLY ALTERING** | Multiple exact-style candidates per instruction | Reduction/tie order must remain deterministic | First vector implementation, gated by differential tests |
| SIMD `float` | **ALTERING** | Twice as many lanes as `double`; GPU-friendly | Near ties can select different colors or patterns | Optional explicitly named fast mode only |
| Fixed-point `int32` | **POSSIBLY ALTERING until proven** | Deterministic, many SIMD lanes, portable to GPUs | Scaling, overflow, `/3`, and diffusion rounding need a formal contract | Research after exact CPU kernel |
| Wider pixel/index storage | Same data, worse density | None for this workload | More memory and cache pressure | Do not pursue |

Runtime ISA selection is essential. A portability layer such as Google Highway
is a serious candidate because it supplies runtime dispatch across x86 SIMD and
Arm NEON/SVE while permitting a scalar target. An alternative is small
compiler-specific dispatched kernels, but maintaining equivalent GCC,
AppleClang, and Windows MinGW behavior would be more project-specific work.

### Estimated effect and output risk

- Merely enabling a newer compiler target and relying on automatic
  vectorization: about **1.05x-1.25x** overall.
- Explicit SIMD `double` candidate kernels: about **1.4x-2.5x** in the hot
  search and **1.2x-1.8x** over the full nine-mode benchmark.
- A proven SIMD fixed-point kernel: potentially **2x-5x** in the hot search,
  but it is **POSSIBLY ALTERING** until byte-for-byte tests pass.

The lower half of the range is more realistic on the current AVX-only Sandy
Bridge-era VM. Modern AVX2 systems and Apple Silicon should offer more headroom.

## 2. Algorithms, caches, and lookup tables

### Exact shared-prefix pattern search — **NON-ALTERING when proven**

The current exhaustive search repeatedly evaluates the same initial bits. For
example, all patterns beginning `010` independently recompute those first three
pixel decisions. A binary prefix tree of depth eight contains 511 nodes, while
256 complete patterns contain 2,048 pixel steps before early exits.

An exact depth-first traversal can carry the partial distance and horizontal
error once per prefix, branch for foreground/background at the next bit, prune
when the partial distance already exceeds the best candidate, and visit leaves
in the current numeric pattern order. Preserving pair order, pattern order, and
strict tie rules preserves deterministic selection.

- Theoretical reduction before pruning: nearly **4x** fewer pixel-step
  evaluations per color pair.
- Expected hot-search gain after accounting for current early exits:
  **1.3x-2.5x**.
- Expected full-benchmark gain: approximately **1.25x-2.0x**.

This is the highest-value exact algorithm to prototype first.

### Direct no-error solution — **NON-ALTERING when proven**

When candidate-dependent error diffusion is disabled, the eight bits in a
pattern no longer depend on each other. For a fixed foreground/background pair,
each bit can directly choose whichever color gives the lower distance at that
pixel. There is no need to enumerate 256 patterns.

This removes up to 256 pattern trials per color pair. Including pair search,
preview construction, and other fixed work, a realistic affected-mode estimate
is **3x-20x**. It greatly accelerates `None` and ordered-only workflows but does
not speed up the default Atkinson path.

### Palette-selection improvements — **POSSIBLY ALTERING unless semantics match**

Median cut currently materializes pixel points and repeatedly sorts selected
blocks. A weighted RGB444 histogram can represent repeated colors once. A
careful implementation can split weighted bins while retaining specified tie
and median behavior. Per-row Scanline F18A palette selection can also run in
parallel before the ordered quantization stage.

Estimated effect is **1.2x-3x** for the palette-selection stage and about
**1.05x-1.3x** for the complete Paletted/Scanline modes. If weighted-bin
splitting changes median, truncation, or tie behavior, the result is
**ALTERING** and must be presented as a different palette-selection mode.

### Useful in-memory tables — **NON-ALTERING when exact**

Small, settings-specific runtime tables can reduce repeated scalar work:

- per-channel contributions to compatible color-distance calculations;
- prepared source samples for no-error paths;
- pattern masks and deterministic traversal orders; and
- the already-used palette and mixed-color prepared samples.

These tables should be regenerated when the palette, distance weights, luma
emphasis, or relevant settings change. Their size and lifecycle remain bounded
to a conversion or session.

### Why disk-based precomputation is a poor primary fit — **not a recommended method**

A universal persisted lookup table is not recommended. The source color,
editable palette, perceptual weights, luma emphasis, dither kernel, incoming
continuous error, candidate pair, and current pixel position all affect the
answer. A table covering enough of that state is either enormous or an
approximation. It would also need platform-neutral versioning and invalidation.

Disk caches may become useful for a future batch workflow at a much higher
level—for example, reusing a completed conversion when the exact source digest
and all settings match—but not as a replacement for the hot color search.

### Content memoization — **NON-ALTERING only with a complete key**

A bounded in-memory cache of repeated eight-pixel blocks can help pixel art,
flat backgrounds, and adjacent animation frames. It is most practical in the
no-error path. With diffusion enabled, relevant incoming error becomes part of
the cache key and lowers the hit rate.

The effect is content-dependent: approximately **1.0x** on noisy photographs
and potentially **2x-5x** on highly repetitive sources. This should be measured
after the core search is improved, not used as the first optimization.

If a cache omits incoming diffusion error, source position, palette state, or a
conversion setting, it is **ALTERING**, even when the resulting preview often
looks identical.

### Approximate search — **ALTERING**

Color-pair shortlists, beam search, coarse-to-fine pattern search, and learned
heuristics can reduce work further. They can also select a different legal
output. If investigated, they belong behind an explicitly named fast/preview
quality mode and must not silently replace the compatibility path.

## 3. Multicore processing

The application already runs a conversion on a worker rather than the UI
thread. That improves responsiveness, but one worker does not make a single
conversion multicore.

### Naturally parallel work — **NON-ALTERING when ordered and deterministic**

| Work | Parallel status |
| --- | --- |
| Multicolor 4x4 blocks | Independent |
| Dual Multicolor 4x4 blocks | Independent |
| Half Multicolor underlay | Independent |
| Bitmap-family blocks with error disabled | Independent |
| Scanline F18A palette selection | Independent by row |
| Different files or animation frames | Independent |
| Default error-diffused selected blocks | Ordered dependency |

Independent stages should use coarse ranges rather than one task per small
block. Coarse tasks amortize scheduling overhead, share read-only prepared
state, preserve result order, and remain cancellable. Batch mode should limit
concurrency by measured core count and memory rather than launching an
unbounded worker per frame.

### Candidate-level parallelism — **NON-ALTERING when reduction is deterministic**

The candidate pairs or patterns for one selected block can be divided among a
persistent worker team and then reduced in deterministic original order. This
retains the inter-block dependency, but thousands of small synchronization
points can erase the benefit. SIMD is likely more efficient at this level.
Candidate-level CPU threading should therefore be prototyped only after the
scalar and SIMD search is measured.

Wavefront tiling across image regions is theoretically possible. The current
right, far-right, down-left, down, down-right, and down-two error propagation
creates a nontrivial tile boundary. It is high complexity for limited exact
parallelism and is not an early recommendation. Unless the tile-boundary
calculation is proven equivalent to the scalar sequence, it is **POSSIBLY
ALTERING**.

### Estimated effect on the four-logical-processor VM

- Default nine-mode benchmark, selective threading only: **1.2x-1.7x**.
- No-error independent modes: **2.5x-3.5x**.
- Many independent images/frames: **3.0x-3.8x** throughput.

These estimates assume the OS and interface retain enough capacity to stay
responsive. Scaling should be measured at one, two, and four workers; logical
processor count is not a guarantee of linear speedup.

## 4. GPU-enhanced conversion

### What maps well to a GPU — **backend-dependent output risk**

Independent blocks and no-error conversion are conventional compute workloads:
one workgroup can own an output block, lanes can evaluate candidate pairs or
patterns, and a deterministic reduction can choose the first minimum.
Animations and batch images provide even more independent work and amortize
pipeline startup.

For default error diffusion, a workgroup may cooperatively evaluate candidates
for the current block, but blocks still advance in dependency order. This uses
less of a large GPU than independent dispatch. A persistent workgroup avoids a
dispatch per block, yet occupancy may remain low.

The source image is only 256x192 pixels—about 147 KiB of RGB—so raw transfer
volume is modest. Pipeline creation, synchronization, data marshaling, and the
serial portions of the pipeline are more important overheads than the transfer
itself.

### Numeric portability is the deciding issue — **POSSIBLY ALTERING by default**

The existing kernel uses `double`. Vulkan exposes 64-bit shader floating point
only as an optional capability, and portable WebGPU/WGSL does not provide a
concrete 64-bit floating or integer type. Consumer GPU double performance also
varies widely.

Therefore a portable GPU backend should use one of these contracts:

1. a formally specified `int32` fixed-point kernel that is also the CPU
   reference (**POSSIBLY ALTERING until parity is proven**); or
2. an accepted `float32` fast-mode kernel (**ALTERING**) whose output
   differences are measured and visible to the user.

The first is preferable if strict deterministic CPU/GPU parity can be achieved.
The fixed-point design must define channel scaling, accumulator bounds, every
division/rounding operation, error-buffer saturation, and deterministic
tie-breaking before shader work begins.

### Cross-platform API choices — **API choice does not determine parity**

| Option | Windows | Linux | macOS | Assessment |
| --- | --- | --- | --- | --- |
| Vulkan compute + MoltenVK | Native Vulkan | Native Vulkan | Vulkan portability over Metal | Strong control and mature compute model; adds MoltenVK packaging and portability-subset testing |
| Dawn/WebGPU Native | D3D12/Vulkan | Vulkan/OpenGL | Metal | Broad backend abstraction and one shader model; sizeable dependency and WGSL numeric limits |
| Qt `QRhi` compute | D3D/other Qt backends | Vulkan/OpenGL | Metal | Convenient Qt integration, but Qt documents QRhi as private API without source or binary compatibility guarantees |
| OpenCL | Vendor-dependent | Vendor-dependent | Deprecated | Not appropriate as the primary Mac-capable backend |
| CUDA, DirectML, Metal-only, SYCL variants | Hardware/platform-specific | Varies | Varies | Possible optional accelerators later, not the universal first backend |

The first proof of concept should compare Vulkan plus MoltenVK against Dawn,
using the same already-validated integer kernel. Backend choice should be based
on executable/package size, startup time, shader tooling, Intel/AMD/NVIDIA and
Apple Silicon coverage, and maintenance cost—not kernel timing alone.

### Estimated GPU effect and output contract

- Independent no-error hot kernel: approximately **10x-50x**.
- Independent no-error end-to-end mode: approximately **3x-20x**.
- Candidate-parallel default error kernel in expensive modes:
  approximately **3x-12x**.
- Complete current nine-mode benchmark after a mature integer GPU path:
  approximately **2.5x-8x**, or roughly 3.2-10.3 seconds from the present
  25.872-second baseline.

These are the widest and least certain ranges in this report. The estimates
assume a mature implementation; they do not make the output parity-safe. A GPU
path is **NON-ALTERING** only after it matches the approved CPU oracle for the
selected settings. Otherwise it is **POSSIBLY ALTERING** or, for an explicitly
reduced-precision path, **ALTERING**. Integrated GPUs,
old drivers, pipeline startup, and small one-image workloads can make the CPU
faster. GPU acceleration must remain optional and selected at runtime only when
a capability check or calibration indicates a benefit.

## Projected benchmark impact

The following rows model opportunities independently unless a combined row is
explicitly named. Factors must not be multiplied blindly because they optimize
overlapping work.

| Proposed state | Output classification | Estimated overall factor | Modeled nine-mode time |
| --- | --- | ---: | ---: |
| Current measured Release | **NON-ALTERING reference** | 1.00x | 25.872 s |
| Newer ISA/runtime dispatch alone | **NON-ALTERING after validation** | 1.05x-1.25x | 20.7-24.6 s |
| Explicit SIMD `double` | **POSSIBLY ALTERING** | 1.2x-1.8x | 14.4-21.6 s |
| Exact shared-prefix search | **NON-ALTERING when proven** | 1.25x-2.0x | 12.9-20.7 s |
| Selective threading alone | **NON-ALTERING when deterministic** | 1.2x-1.7x | 15.2-21.6 s |
| Shared-prefix + SIMD + selective threading | **POSSIBLY ALTERING until all kernels pass** | **2.2x-5.0x** | **5.2-11.8 s** |
| Mature fixed-point GPU path | **POSSIBLY ALTERING until oracle parity** | **2.5x-8.0x** | **3.2-10.3 s** |

One reasonable planning point for the combined exact CPU implementation is
about 3.4x overall:

| Mode | Baseline (s) | Planning factor | Modeled time (s) |
| --- | ---: | ---: | ---: |
| Bitmap 9918A | 6.814 | 3.5x | 1.947 |
| Greyscale Bitmap 9918A | 3.350 | 3.0x | 1.117 |
| Black-and-White Bitmap 9918A | 0.381 | 2.0x | 0.191 |
| Multicolor 9918 | 0.259 | 3.0x | 0.086 |
| Dual Multicolor 9918 | 0.272 | 3.0x | 0.091 |
| Half Multicolor 9918A | 5.432 | 3.5x | 1.552 |
| Bitmap Color Only 9918A | 0.333 | 2.0x | 0.167 |
| Paletted Bitmap F18A | 4.645 | 3.5x | 1.327 |
| Scanline Palette Bitmap F18A | 4.385 | 4.0x | 1.096 |
| **Total** | **25.872** | **3.42x** | **7.574** |

This table is a prioritization model. It is not evidence that each mode will
reach that factor.

## Compatibility and measurement rules

No optimization is “non-altering” merely because it produces a visually close
PNG. The compatibility claim is about target tables, palette bytes, preview
pixels, diagnostics, and deterministic tie choices. A method may be promoted
from **POSSIBLY ALTERING** to **NON-ALTERING** only after the scalar reference
and optimized implementation agree over the complete golden corpus plus
adversarial equal-distance, near-tie, edge, cancellation, and randomized cases.

If that proof fails, the implementation must either remain an opt-in
**ALTERING** path or be redesigned. It must not silently become the default
compatibility backend.

Every optimization must be classified before implementation:

- **Exact/parity path:** all target tables, palette data, and preview pixels
  match the scalar reference byte-for-byte.
- **Numerically changed but intended path:** differences are understood,
  quality-tested, named in the interface, and never silently selected for a
  compatibility conversion.

Tests must include adversarial equal-distance and nearly equal-distance cases,
not only normal images. Parallel reductions must select the same first minimum
as the current candidate order. Fast floating-point compiler flags make
cross-compiler validation especially important.

Benchmark reports should separate:

- end-to-end latency for one source;
- conversion-kernel time;
- palette-selection time;
- startup, shader compilation, and transfer time;
- warm and cold cache behavior;
- interactive cancellation latency; and
- throughput for many independent sources/frames.

Each result must record CPU, logical processor count, GPU and driver, OS,
compiler, build flags, selected backend, settings, and source corpus. Comparing
only totals can hide a regression in a common mode behind a gain in an
infrequent mode.

## Proposed implementation program

### Stage 0 — Extend measurement before optimization

- Add stage timers around decode, transform, adjustment, palette selection,
  quantization, preview construction, and export.
- Add microbenchmarks for one eight-pixel search block.
- Benchmark default Atkinson, no error, ordered-only, and both color-distance
  families.
- Use photo, pixel-art, flat-color, noise, and future multi-frame corpora.
- Capture profiler, branch, cache, and compiler vectorization evidence.
- Preserve the 2026-09-20 Release result as the comparison baseline.

Exit criterion: repeatable stage results with variance and an identified hot
kernel for each expensive mode.

### Stage 1 — Exact scalar algorithm work — **NON-ALTERING target**

- Isolate a small scalar reference search kernel.
- Implement the direct no-error solver.
- Prototype prefix-tree traversal for the error-enabled search.
- Preserve pair, pattern, reduction, and tie order.
- Run all golden output plus randomized/adversarial differential tests.

Go/no-go target: at least 1.4x in expensive modes with byte-identical output.

### Stage 2 — Portable CPU SIMD — **POSSIBLY ALTERING until validated**

- Prototype Google Highway runtime targets and compare dependency cost with
  project-owned compiler-dispatched kernels.
- Convert candidate state to structure-of-arrays storage.
- Implement `double` lanes first and deterministic lane reduction.
- Keep the scalar kernel as fallback and oracle.
- Verify x86 scalar/SSE/AVX/AVX2 where available and Arm NEON on both Mac
  architectures represented by CI or test hardware.

Go/no-go target: at least 1.5x over the improved scalar kernel on representative
modern x86 and Arm hardware, without output differences.

### Stage 3 — Coarse multicore work — **NON-ALTERING target**

- Parallelize independent modes and stages with bounded coarse ranges.
- Add frame/file-level scheduling to the documented Batch Mode architecture.
- Retain deterministic output order, cancellation, and UI responsiveness.
- Avoid task-per-block scheduling in the default error-diffused path.

Go/no-go target: at least 2.5x throughput on a four-logical-processor independent
workload and no single-image regression.

### Stage 4 — Specify and validate fixed point — **POSSIBLY ALTERING until proven**

- Derive accumulator ranges and a Q-format for distance and error values.
- Define rounding for all divisions and error-distribution steps.
- Implement a scalar integer reference before SIMD or shaders.
- Differential-test normal and adversarial inputs against the compatibility
  reference.
- Decide whether the result is exact compatibility or an explicit fast mode.

Exit criterion: a stable, documented numeric contract suitable for two
independent implementations.

### Stage 5 — GPU proof of concept — **POSSIBLY ALTERING until oracle parity**

- Implement only the Stage 4 kernel, not a separate GPU-only algorithm.
- Compare Vulkan/MoltenVK and Dawn integration with one expensive mode first.
- Cache pipelines and test cold and warm startup.
- Test integrated and discrete Intel/AMD/NVIDIA hardware, Intel Macs, and Apple
  Silicon Macs where available.
- Provide automatic capability detection and immediate CPU fallback.

Go/no-go target: at least 3x end-to-end on expensive modes after startup, with
no regression on machines where GPU selection is disabled.

### Stage 6 — Product integration — **preserve both contracts explicitly**

- Add `Auto`, `CPU`, and `GPU` backend choices under Advanced Settings only
  after backend selection is reliable.
- Show the active backend in diagnostics and benchmark output.
- Add backend-specific CI/scheduled tests and package all required runtime
  licenses.
- Fall back safely after device loss, shader failure, or unsupported features.

## Recommended decision

Proceed with Stages 0-3 as the next performance program. They offer the best
chance of large, cross-platform gains while retaining legacy-compatible output.
Treat Stage 4 as a formal research gate for Stage 5. Do not invest in persistent
disk lookup tables or a GPU backend until the smaller exact search and numeric
contract are established.

The compatibility default should remain the scalar reference plus only the
optimized kernels that have earned the **NON-ALTERING** label. Any faster path
that has not earned that label should be exposed as a clearly named alternative,
with its output contract and compatibility differences visible in diagnostics.

This sequence also supports the future Batch Mode design: the exact CPU kernel
improves per-frame latency, multicore scheduling improves CPU throughput, and a
GPU backend can later process large independent frame queues efficiently.

## External technical references

- [Google Highway](https://github.com/google/highway) — portable SIMD targets,
  runtime dispatch, and supported x86/Arm instruction families.
- [GCC common function attributes](https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html)
  — compiler multiversioning and `target_clones` where supported.
- [Qt `QThreadPool`](https://doc.qt.io/qt-6/qthreadpool.html) — bounded reusable
  worker pool and ideal-thread-count behavior.
- [Vulkan specification](https://registry.khronos.org/vulkan/specs/latest-ratified/pdf/vkspec.pdf)
  — optional shader feature capabilities including 64-bit floating point.
- [MoltenVK](https://github.com/KhronosGroup/MoltenVK) — Vulkan portability over
  Metal on Intel and Apple Silicon Macs.
- [Dawn](https://dawn.googlesource.com/dawn) — native WebGPU implementation
  over D3D12, Metal, Vulkan, and OpenGL backends.
- [WebGPU Shading Language](https://gpuweb.github.io/gpuweb/wgsl/) — portable
  shader type system and numeric-type limits.
- [Qt `QRhi`](https://doc.qt.io/qt-6/qrhi.html) — Qt rendering hardware
  interface and its private-API compatibility warning.
- [Apple OpenCL](https://developer.apple.com/opencl/) — macOS deprecation and
  Metal migration direction.
- [Intel Xeon E5-2690 specification](https://www.intel.com/content/www/us/en/products/sku/64596/intel-xeon-processor-e52690-20m-cache-2-90-ghz-8-00-gts-intel-qpi/specifications.html)
  — current benchmark host AVX capability.
