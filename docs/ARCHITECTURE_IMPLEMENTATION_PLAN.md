# Architecture implementation plan

Status: active migration plan, 2026-10-01.

Implementation checkpoint (2026-10-01): the Stage 8 proving slice now
registers implemented V9938 and V9958 profiles, native SCREEN 5–8 and
SCREEN 10–12 descriptors, Yamaha framebuffer/palette regions, and a portable
registered compiler path. Sprite asset compilation and command/register-state
export remain subsequent Stage 8 work.

Implementation checkpoint (2026-10-01): the planar-target seam now registers
the Sega Master System VDP and native 192-, 224-, and PAL 240-line Mode 4
descriptors. Its portable compiler emits bounded 4-plane tile data, flip-aware
name-table attributes, two RGB222 CRAM banks, and initial VDP register state;
the GUI, CLI, project schema, preview, validation, and RAW exporter all consume
the same registration.

Implementation checkpoint (2026-09-30): the first compatibility seam is now
implemented. The repository has validated stable ID value types, TMS9918A and
F18A display-mode, character-pattern, sprite, and mode-option descriptors,
target/mode registry validation, enum adapters
for target modes, export formats, and memory-region roles, a canonical recipe
fixture exercised by both GUI-facing and CLI workflows, and a CTest guard that
prevents Qt dependencies from entering portable core/format modules. Existing
converter entry points and version-1 recipe fields remain compatibility APIs.
The first project-session presentation seam is also active: project name and
configured targets are persisted additively, active-target selection lives in
the Project Bar, Source is common to all modes, and tabbed versus split layouts
own mode selection according to the accepted interface contract.
Screen Image, Character, and Sprite option panels now consume that one active
target and render only descriptor-approved controls.
The shared Source surface now owns target-independent raster preparation tools,
selection and floating placement state, and system-font text; target grids,
hardware font formats, conversion rules, and native output formats remain on
target-owned surfaces.

This plan turns the accepted architecture in [ARCHITECTURE.md](ARCHITECTURE.md)
into small, releasable changes. It supersedes architecture sequencing embedded
in older phase checklists and feature proposals. It does not authorize a
big-bang rewrite or change existing output merely to make code movement easier.

## Program outcome

The migration is complete when RetroVDP Studio can add a new target display
mode by registering a descriptor and its compiler, validator, preview, and
applicable formats; when GUI and CLI use the same project/application services;
and when a project can own independent managed outputs derived from shared
source assets.

V9938 is the first proving target after the framework gates. The target catalog
and later hardware waves remain in
[VDP_SUPPORT_ROADMAP.md](VDP_SUPPORT_ROADMAP.md).

## Constraints

- Work directly on `main` unless repository protection requires a pull request.
- Preserve existing TMS9918A/F18A bytes, diagnostics, CLI names, stable target
  IDs, and supported version-1 recipes unless a reviewed migration says
  otherwise.
- Keep the application buildable and releasable after every work package.
- Add seams before moving code through them.
- Do not create abstract base classes for target families.
- Do not split a library solely to make the tree resemble the target diagram;
  split when dependency enforcement or ownership becomes clearer.
- Character and sprite editor work already present remains usable throughout
  the migration.

## Baseline and evidence

Before the first code change, record a green build/test result on each required
CI platform and retain the current golden corpus. Add a lightweight architecture
check that prevents Qt headers from entering portable include/source folders.

The 2026-09-30 audit measured the immediate concentration points:

- `ImageInputController`: 89 QML properties and 168 out-of-line method
  definitions;
- `EditorProjectController`: 43 QML properties, 45 invokable methods, private
  character/sprite domain structs, histories, and recipe persistence;
- one application-level conversion switch selecting all nine algorithms;
- target layouts and export applicability selected by separate mode switches;
- GUI recipe writing and CLI recipe reading implemented in different
  frontends; and
- a palette value capped globally at sixteen entries.

These are migration indicators, not standalone quality thresholds. A smaller
file is not a success if responsibilities remain coupled.

## Workstream map

| Workstream | Current owner | Target owner |
| --- | --- | --- |
| IDs and hardware facts | numeric enums and `TargetProfile` | validated stable IDs and immutable descriptors |
| Conversion orchestration | `app/ConversionPipeline.cpp` | portable compile application service plus registered strategies |
| Character/sprite data | private controller structs | portable asset/project model |
| Recipe schema | GUI writer and CLI reader | one serializer/migration boundary |
| Target output | fixed table-role enum | role-ID artifact regions with placement metadata |
| Format applicability | central export enum/switch | format registry over target/mode/asset/region contracts |
| GUI state | two broad controllers | project session plus workspace presentation models |
| Background publication | image controller | application job service with presentation adapter |

## Stage 0 — Protect current behavior

Goal: make later movement diagnosable.

Work packages:

1. Add characterization tests for target/profile/mode lookup, defaulting, table
   layouts, export applicability, and recipe round trips.
2. Add one canonical recipe fixture consumed by both the GUI-facing service and
   CLI test path.
3. Add a dependency check for Qt includes under portable modules.
4. Split oversized test files only where fixture ownership becomes clearer;
   do not change coverage merely for line count.
5. Document the stale pre-rebrand build-cache symptom in developer guidance or
   make preview scripts detect and explain it.

Exit gate:

- all current tests and golden fixtures pass;
- registry, schema, and applicability behavior has direct coverage; and
- CI can detect a Qt dependency entering a portable module.

## Stage 1 — Stable IDs and display-mode descriptors

Goal: remove numeric identity and global geometry assumptions from public
boundaries without replacing working converters.

Work packages:

1. Introduce validated `TargetId`, `ModeId`, `FormatId`, and `RegionRoleId`
   values with hash/equality support and bounded parsing.
2. Add `TargetKind`, rational pixel aspect, mode geometry, character-pattern
   geometry, palette model, memory-region, and supported-asset descriptors.
3. Build an immutable target registry that validates duplicate IDs and
   descriptor contradictions.
4. Register TMS9918A and F18A descriptors using current facts. Keep V9938
   planned but do not register strategies that do not exist.
5. Adapt current enums to IDs at one compatibility boundary. Version-1 recipes
   and existing CLI names resolve through that boundary.
6. Make image preparation obtain geometry and palette requirements from the
   selected mode/compiler plan. Keep PowerPaint's 240x160-in-256x192 behavior
   as a named mode-specific preparation option.
7. Remove target width/height from general conversion settings when all callers
   use mode geometry, or retain them only in an explicitly custom preview
   contract.

Exit gate:

- current conversions still produce identical target bytes;
- saved projects and CLI requests use stable IDs at their boundaries;
- registry contract tests reject duplicate and invalid descriptors; and
- no GUI or CLI switch is required to display target/mode lists.

## Stage 2 — General target artifacts and format registry

Goal: make encoded output and export applicability extensible before adding a
new target.

Work packages:

1. Add a target artifact made of stable-role regions with optional address,
   bank/page, alignment, source asset, and mode metadata.
2. Adapt current `TargetMemoryImage` tables to the artifact contract and retain
   the old API temporarily for golden parity.
3. Move expected-region layouts from the global conversion-mode switch into
   mode registrations.
4. Introduce format descriptors for visual decode, structured import, and
   export capabilities. Applicability is declared by IDs, asset kinds, and
   required roles.
5. Register existing RAW, RLE, TIFILES, V9T9, MSX, Coleco, Adam, Extended
   BASIC, ROM, and PNG operations.
6. Keep byte encoders pure and PNG/filesystem behavior in the Qt adapter.
7. Add registry-wide contract tests and pairwise applicability fixtures.

Exit gate:

- all current export golden files are byte-identical;
- adding a test format does not require editing the GUI, CLI, or a central
  applicability switch; and
- an exporter rejects missing/invalid artifact roles before any file opens.

## Stage 3 — Portable project and asset model

Goal: move authored data out of Qt controllers.

Work packages:

1. Define source, palette, character/pattern, tile-map, sprite-pattern,
   sprite-instance, composite/object, and display-state assets as bounded
   standard-C++ values.
2. Move character and sprite set data from private controller structs into the
   asset model without changing presentation behavior.
3. Add project metadata, target configurations, selection-independent asset
   collections, reservations/locks, and dirty state.
4. Represent changes as project transactions so a compound import, pan,
   allocation, or remap is one undo unit.
5. Preserve the existing visible character and sprite histories through an
   adapter while converging on project-level undo/redo.
6. Add invariant tests for indexes, sizes, color depth, placement, allocation,
   and target compatibility.

Exit gate:

- character and sprite assets can be created, edited, copied, and validated in
  a headless unit test;
- controllers no longer define authoritative asset structs; and
- current editor workflow tests pass without QML schema changes.

## Stage 4 — One project serializer and migration boundary

Goal: make persistence a shared service rather than frontend code.

Work packages:

1. Define a versioned project envelope and explicit load limits.
2. Create one serializer interface and one initial JSON adapter. The adapter
   may use Qt JSON, but it consumes and produces portable project values.
3. Move version-1 recipe reading/writing and compatibility aliases into the
   serializer.
4. Make GUI and CLI call the same serializer/application use case.
5. Preserve unknown future fields where possible or refuse a destructive save
   with a clear diagnostic.
6. Add malformed, oversized, unknown-ID, old-version, future-version, and
   atomic-save fixtures.
7. Decide and document whether the long-term extension remains
   `*.rvdp.json`; changing it is not required for this stage.

Exit gate:

- one fixture round-trips identically through GUI and CLI service paths;
- direct `QJson*` schema knowledge is absent from the two frontends; and
- every supported schema migration has a fixture and deterministic diagnostic.

## Stage 5 — Application use cases and thin presentation models

Goal: separate workflow policy from QML exposure.

Work packages:

1. Introduce use cases for open/import, compile, validate, preview,
   save/load, and export.
2. Move generation/cancellation policy into a frontend-neutral job service.
3. Extract source preparation and screen-image settings from
   `ImageInputController` behind an application session.
4. Extract character, sprite, diagnostics, and export presentation models one
   workspace at a time. Do not rename the entire QML API in one change.
5. Move QML lists and enablement to descriptor-derived view data.
6. Move CLI mode, target, and format choices to registry enumeration while
   preserving exact command names and exit codes.
7. Narrow `retrovdp_app_support`; portable use cases must not link Qt Gui or
   image-I/O adapters.

Exit gate:

- a headless integration test compiles and exports through application use
  cases without QML or a QObject controller;
- controllers hold presentation/selection state rather than project data or
  conversion policy; and
- GUI and CLI behavior remains compatible.

## Stage 6 — Registered compiler, validator, and preview strategies

Goal: remove the last central target dispatch and make encoded output the
shared truth.

Work packages:

1. Define narrow compile, validate, and preview strategy signatures.
2. Register adapters for all current bitmap, multicolor, and F18A algorithms.
3. Extract common preprocessing into compiler plans selected by mode.
4. Make validation consume artifacts and typed descriptors.
5. Make preview render the encoded artifact where the current mode permits;
   record any temporary source-preview compatibility path explicitly.
6. Group implementations under TI 9918 and F18A target-family folders only
   after registrations remove include cycles.
7. Delete obsolete enum dispatch and compatibility adapters when no supported
   caller uses them.

Exit gate:

- every implemented mode has all three registered strategies;
- no application-wide conversion-mode switch chooses a concrete converter;
- preview, validation, and export agree on the same artifact; and
- current golden conversion and preview fixtures pass.

## Stage 7 — Managed multi-target outputs

Goal: implement the product's durable source-to-many-target model.

Work packages:

1. Add project-owned managed outputs keyed by source asset, target, and mode.
2. Record settings, hashes, generated regions, diagnostics, preview state,
   manual edits, and export history.
3. Mark outputs stale when sources or relevant settings change.
4. Require explicit confirmation before regeneration replaces manual edits.
5. Add create-from-screen-result, import-current-palette, and
   create-target-variant use cases.
6. Add deployment manifests that can collect several native exports without
   allowing exporters to perform compilation.
7. Add project recovery and bounded autosave after the stable serializer is in
   place.

Exit gate:

- one source can own independent TMS9918A and F18A outputs;
- rebuilding one never mutates the other;
- save/reopen preserves stale/manual-edit state; and
- export manifests are reproducible from validated artifacts.

## Stage 8 — V9938 proving slice and cleanup

Goal: prove the architecture with real variation rather than synthetic
abstraction.

Work packages:

1. Verify primary Yamaha references and approve the first mode/variant scope.
2. Register V9938 descriptors, larger memory regions, programmable palette,
   and Sprite Mode 2 facts needed by that slice.
3. Reuse only explicitly compatible TI/Yamaha stages.
4. Implement one 256-pixel native 4-bit bitmap mode end to end.
5. Add byte-level, validation, preview, import/export, and project fixtures.
6. Remove transition adapters made obsolete by the proving implementation.
7. Review module size, dependency graph, diagnostics, and documentation before
   beginning the next VDP support wave.

Exit gate:

- the first V9938 mode works from source/project through native export;
- its addition did not require target-name branches in QML or the CLI; and
- current TMS9918A/F18A suites remain green.

## Change sequencing inside a stage

Each work package should normally land in this order:

1. contract and characterization test;
2. new value or seam beside the existing path;
3. one migrated consumer;
4. parity verification;
5. remaining consumers;
6. deletion of the obsolete path; and
7. documentation update in the same change that alters the contract.

Temporary adapters must have an issue or checklist item naming their removal
gate. “Both systems indefinitely” is not a migration strategy.

## Quality gates

Every stage must retain:

- build and tests on Windows, Linux, Intel macOS, and Apple Silicon macOS;
- deterministic core output and approved golden hashes;
- bounded parsing/allocation at file and project trust boundaries;
- atomic save/export behavior;
- cancellation and newest-generation publication behavior;
- no Qt dependency in portable modules;
- accessible GUI behavior and stable automation names; and
- release documentation synchronized with implemented behavior.

## Explicit non-goals

- No universal intermediate representation that erases meaningful differences
  between tile, framebuffer, character, and object hardware.
- No target inheritance tree.
- No external binary plug-in ABI in this migration.
- No database or cloud service for project storage.
- No V9938 implementation before Stages 1, 2, and 6 provide the required
  contracts.
- No removal of specialized optimized converters solely to maximize reuse.

## Immediate next increment

Continue Stage 1 by moving image preparation to the registered mode geometry
and palette constraints. Preserve PowerPaint's 240x160 framing as a named
preparation policy, keep numeric version-1 recipe fields readable, and remove
the remaining GUI/CLI target-list duplication only after parity tests cover the
new registry-backed presentation adapters. This completes the descriptor seam
before Stage 2 generalizes target artifacts and export applicability.
