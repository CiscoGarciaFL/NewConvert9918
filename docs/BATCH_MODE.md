# Future batch mode: video, slideshow, and animation

## Status and intent

This document specifies a future feature. It does not describe functionality
available in the current application and does not authorize adding a media
dependency yet.

Batch Mode will turn a time-ordered or explicitly ordered source into an
enumerated sequence of still frames, then apply one snapshot of the normal New
Convert 9918 conversion settings to each frame. The same conversion core,
target modes, validation, and exporters used for a single image must be used
for every batch item.

The initial goal is deterministic frame-sequence production, not video
editing, audio handling, or direct encoding of a new movie file.

## Supported source families

The design must accommodate these source families through a common frame
enumerator:

1. **Video files** — containers and codecs supported by a separately selected
   cross-platform media backend. Likely examples include MP4, MOV, AVI, MKV,
   WebM, MPEG, and WMV, but the final list depends on the selected backend and
   its licensing/package policy.
2. **Animated images** — animated GIF first, followed by animated WebP and APNG
   when the installed decoder exposes their frames and timing reliably.
3. **Slideshow folders** — a directory of still images ordered by natural
   filename order, an explicit file list, or a small manifest that specifies
   order and duration.
4. **Numbered image sequences** — files such as `scene_0001.png` through
   `scene_0240.png`, including sequences with missing numbers when the user
   chooses to permit gaps.
5. **Selected still images** — an explicitly ordered multi-selection, useful
   for converting artwork sets without creating a slideshow manifest first.

Audio, subtitles, chapter data, and other non-image streams are ignored. The
application must report that policy before processing a source containing
them.

## Common frame model

Every source is normalized into a stream of `FrameRecord` values before
conversion. The eventual implementation should keep the public model
independent of FFmpeg, Qt Multimedia, or any other decoder API.

Each frame record needs at least:

- a zero-based internal ordinal;
- a one-based display/export number;
- the source file and source-stream identity;
- the original frame index when the decoder provides one;
- presentation timestamp and duration when meaningful;
- decoded width, height, pixel format, color-space information, and alpha;
- a bounded `RgbImage` containing the normalized pixels;
- warnings generated while decoding or normalizing the frame.

Video and animated-image frames use their source timing. Slideshow frames use
the manifest duration or a user-selected default. Numbered still sequences may
omit timing when they are intended only as ordered conversion jobs.

Frame enumeration must be stable. Given the same source, source options, and
decoder version, frame numbers and output names must not depend on thread
scheduling or completion order.

## Source range and sampling

Before conversion, the user can restrict what is enumerated by:

- first and last frame;
- start and end timestamp for timed media;
- every Nth source frame;
- a target sampling rate such as 10, 15, 24, or 30 frames per second;
- a maximum output-frame count;
- duplicate-frame suppression, which is off by default because it changes the
  relationship between frame numbers and source timing.

Sampling must use presentation timestamps rather than assuming that all video
is constant-frame-rate. Variable-frame-rate input retains an explicit timing
table in the batch manifest.

## Conversion-settings snapshot

Starting a batch freezes a complete copy of the active conversion state:

- conversion and dither modes;
- perceptual matching, error distribution, gamma, histogram, and color-shift
  settings;
- framing, scaling filter, offsets, and background fill;
- editable working palette and F18A palette-selection options;
- PowerPaint framing and export-format selection.

Edits made in the main window after the batch starts do not alter an in-flight
job. A future job editor may support per-frame overrides, but those are outside
the first Batch Mode release.

All frames are independently converted with this settings snapshot. This makes
the initial implementation parallelizable and keeps it aligned with one-shot
GUI and CLI conversions.

### Palette stability

Adaptive F18A palettes can visibly change between adjacent frames even when
the source changes only slightly. Batch Mode therefore needs an explicit
palette policy:

- **Independent** — select the best palette for every frame. This is the
  simplest policy and the initial default, but it may shimmer during playback.
- **Lock to first frame** — select once from the first enumerated frame and use
  that palette throughout the batch.
- **Lock to reference frame** — select from a user-chosen frame.
- **Sequence palette** — select one palette from bounded samples across the
  sequence. This requires a separately specified and tested sampling
  algorithm and may be deferred.

The ordinary editable 9918A working palette is already stable because the
settings snapshot contains its colors.

## Output layout and naming

The default output is a new batch directory. It contains a machine-readable
batch manifest and one directory per enumerated frame:

```text
output-name/
  batch.json
  frame_000001/
    frame_000001.png
    frame_000001.TIAP
    frame_000001.TIAC
  frame_000002/
    frame_000002.png
    frame_000002.TIAP
    frame_000002.TIAC
```

The actual files inside each frame directory follow the selected existing
export format and its applicability rules. A converted-preview PNG is optional
unless PNG itself is the chosen export. Padding width is based on the final
enumerated-frame count, with six digits as the minimum so lexical order equals
frame order.

The batch manifest records:

- application and manifest-schema versions;
- source identity and source options;
- a digest of the frozen conversion settings;
- palette policy;
- requested export format;
- every output frame's ordinal, source index, timestamp, duration, status,
  warnings, and generated files;
- enough timing information for a later playback or packaging tool to
  reconstruct the intended sequence.

No output filename may be derived directly from untrusted container metadata.
Names are sanitized using the same cross-platform policy as ordinary exports.

## Processing model

Enumeration and conversion form a bounded streaming pipeline:

```text
source reader → frame normalizer → bounded queue → conversion workers → writer
```

The decoder must not load an entire movie into memory. Queue depth, decoded
dimensions, encoded source size where knowable, frame count, and aggregate
output estimates all require configurable limits. Conversion workers may run
in parallel, but the writer and manifest must commit results in enumeration
order.

Live preview is optional. When enabled, the Batch view shows the most recently
selected or most recently completed frame and may reuse the progressive-row
preview callback. Disabling it must avoid intermediate PNG encoding just as it
does for one-shot conversion.

Cancellation stops decoding new frames, requests cancellation from active
conversions, finishes or removes temporary writes safely, and records an
incomplete batch state. A resume operation may skip a frame only after its
manifest entry, settings digest, and output digests have been verified.

## Desktop workflow

The proposed entry point is **File → Batch Mode…**. A dedicated Batch view or
dialog should provide:

1. source type and source selection;
2. detected stream/frame/timing summary;
3. frame range and sampling controls;
4. output folder and naming preview;
5. conversion-settings summary with an explicit “Use current settings” action;
6. palette-stability policy where applicable;
7. estimated frame count, output size, and overwrite conflicts;
8. Start, Pause/Resume where supported, and Cancel actions;
9. progress for enumeration, conversion, and writing, including the current
   frame number and failures.

The user should be able to inspect several enumerated source frames before
starting. Batch Mode must not silently change the main window's active source
or conversion settings.

## Command-line direction

The headless interface should eventually expose the same feature without
inventing a second pipeline. A possible shape is:

```text
newconvert9918-cli batch --input movie.mp4 --output converted-frames \
  --mode bitmap-9918a --format tifiles --start 00:00:05 --fps 15
```

Exact syntax is deferred until the source/backend decisions are complete.
Machine-readable progress and the final batch manifest are required for
automation. The CLI must never prompt for overwrite or decoder choices.

## Errors and continuation policy

The default policy is **stop on the first failed frame**. An explicit
continue-on-error option may record failed frames and proceed, but it must
preserve their numbers so later frames do not shift. Decoder warnings,
conversion diagnostics, and write failures remain distinguishable in the
manifest.

Preflight checks occur before the first committed output where possible:

- source readability and stream selection;
- range and sampling validity;
- conversion-settings validation;
- export-format applicability;
- destination writability and overwrite conflicts;
- conservative disk-space estimate.

## Dependency and packaging decision

Video requires a cross-platform decoding backend that the current application
does not ship. Before implementation, compare at least:

- FFmpeg libraries or an FFmpeg subprocess boundary;
- Qt Multimedia and its platform backends;
- licensing, codec availability, binary size, update policy, hardware-decoder
  consistency, and Windows/Linux/macOS packaging behavior.

Deterministic software decoding is preferred for golden tests. Hardware
decoding may be offered later only if pixel differences are documented and do
not affect reproducible batch output unexpectedly.

## Acceptance criteria for a future implementation

- The same frame source produces stable enumeration and names.
- Animated GIF timing, disposal, transparency, and frame composition are
  correct rather than treating frames as independent rectangles.
- Variable-frame-rate video sampling follows presentation timestamps.
- Slideshow natural ordering is defined and tested across platforms.
- Every frame uses the frozen conversion-settings snapshot and existing core.
- Sequential and parallel runs produce byte-identical ordered outputs.
- Cancellation cannot leave a file reported as complete when it is partial.
- Resume verifies settings and file digests before skipping work.
- Malformed, oversized, or extremely long media fails within configured
  limits.
- A batch containing one still image matches ordinary one-shot conversion.
- Windows, Linux, Intel Mac, and Apple Silicon packages expose equivalent
  decoding behavior for the formats claimed by that release.

## Deliberately deferred features

- audio conversion or preservation;
- subtitle rendering;
- timeline editing, transitions, titles, or effects;
- direct AVI/MP4/MOV output;
- automatic generation of target-machine playback programs;
- inter-frame compression or delta encoding for 9918A/F18A data;
- live capture from cameras or screens;
- distributed conversion across multiple machines.

Those may build on the ordered frame and timing manifest later without being
part of the initial Batch Mode contract.
