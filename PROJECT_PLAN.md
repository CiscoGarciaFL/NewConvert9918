# RetroVDP Studio project plan

Status: active roadmap, updated 2026-09-30.

This is the concise program-level plan. Architectural boundaries are defined
in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), and the ordered engineering
migration is defined in
[docs/ARCHITECTURE_IMPLEMENTATION_PLAN.md](docs/ARCHITECTURE_IMPLEMENTATION_PLAN.md).
Feature documents define behavior but do not maintain competing implementation
phase lists.

## Mission

RetroVDP Studio creates, edits, validates, and exports graphics assets for
projects that target classic video hardware. Reusable source material can
produce independent screen-image, character/pattern-map, palette, and
sprite/object outputs for several target hardware profiles. Each output obeys
the selected hardware rules and can be exported manually or collected into a
deterministic deployment manifest.

## Product invariants

- Sources are reusable; target outputs are independently owned.
- The target's encoded artifact is the truth for validation, preview, and
  export.
- Target and format support is registered through stable IDs and typed
  descriptors.
- Project and hardware models remain usable without Qt/QML.
- Native import preserves structure only when the active target can represent
  it honestly.
- Destructive regeneration, overwrite, downgrade, or data loss is explicit.
- Existing TMS9918A/F18A output and supported recipe/CLI contracts remain
  compatible through architecture migration.

## Delivered foundation

The following program is implemented and maintained rather than planned again:

- a portable C++20 conversion core for the audited TMS9918A and F18A
  conversion modes;
- bounded common-raster, PCX, and supported retro-format input;
- deterministic target-table generation and golden fixtures;
- RAW, RLE, TIFILES, V9T9, MSX, Coleco, Adam, Extended BASIC, ROM, and PNG
  export with manifest preflight and atomic writes;
- desktop source preparation, live conversion, settings, previews, and export;
- headless CLI conversion/export with stable diagnostics and exit codes;
- initial Character and Sprite editor surfaces, pattern editing, placement,
  clipboard exchange, undo/redo, and version-1 recipes;
- cross-platform build, test, package, and release procedures; and
- recorded baseline, compatibility, performance, and release evidence.

Completed migration checklists are retained only as historical verification
records where they still explain compatibility decisions. They are not active
roadmaps.

## Current program — architecture modernization

The next program is the staged migration in the architecture implementation
plan:

1. protect current behavior with registry/schema/applicability contracts;
2. introduce stable IDs and typed display-mode descriptors;
3. generalize target artifacts and format registration;
4. extract a portable project/asset model and one serializer;
5. move workflow into application use cases and thin the Qt controllers;
6. register compiler, validator, and preview strategies;
7. add managed multi-target outputs; and
8. prove the design with the first V9938 vertical slice.

The work is incremental. The current GUI, CLI, converters, and recipes remain
available at every stage.

## Feature programs

These specifications feed the architecture program and then continue through
vertical product slices:

| Program | Authority | Current state |
| --- | --- | --- |
| Target/project model | [docs/TARGET_PROFILES.md](docs/TARGET_PROFILES.md) | Model agreed; managed multi-target output not implemented |
| Hardware target catalog | [docs/VDP_SUPPORT_ROADMAP.md](docs/VDP_SUPPORT_ROADMAP.md) | TMS9918A/F18A implemented; V9938 next after framework gates |
| Character and sprite authoring | [docs/VDP_DESIGN_TOOLS.md](docs/VDP_DESIGN_TOOLS.md) | Editor foundations implemented; portable project model, allocation, validation, and full export remain |
| Batch/frame sequences | [docs/BATCH_MODE.md](docs/BATCH_MODE.md) | Deferred until single-asset project/output contracts are stable |
| Release program | [docs/RELEASE_PROCESS.md](docs/RELEASE_PROCESS.md) | Beta pipeline active; stable-release gates remain |
| Performance improvements | [docs/CONVERSION_ACCELERATION_RESEARCH.md](docs/CONVERSION_ACCELERATION_RESEARCH.md) | Research backlog; parity and architecture gates take priority |

## Milestones

### A — Extensible registry foundation

Complete architecture Stages 0–2. Existing output is unchanged, but target,
mode, region, and format identity is stable and descriptor-driven.

### B — Shared project core

Complete Stages 3–4. Character, sprite, and source data are portable project
assets; GUI and CLI use one versioned serializer.

### C — Application boundary

Complete Stages 5–6. Frontends are adapters over shared use cases, and every
implemented mode registers compile, validate, and preview behavior.

### D — Multi-target campaign workflow

Complete Stage 7. A project owns shared sources and independently editable,
stale-aware outputs for several targets, with reproducible native deployment
manifests.

### E — V9938 proving release

Complete the first Stage 8 vertical slice and its hardware/reference review.
Use the result to approve or revise the remaining support waves.

## Cross-cutting quality gates

- Deterministic and golden output remains intentional and reviewed.
- Untrusted input and project data is bounded before allocation.
- Core/project/target modules build without Qt.
- UI enablement is derived from descriptors rather than chipset-name branches.
- GUI and CLI consume the same schema and application contracts.
- Export never performs hidden conversion or repair.
- Save/export operations are atomic and preflight collisions.
- Background work is cancellable and cannot publish a stale generation.
- Every implemented target slice includes descriptor, compiler, validator,
  preview, exporter, and project round-trip coverage as applicable.

## Key risks

| Risk | Control |
| --- | --- |
| Big-bang refactor destabilizes proven conversion | Add seams beside old APIs, migrate one consumer, verify parity, then delete |
| Generic model erases hardware differences | Normalize asset kinds and contracts, not target-specific encoding rules |
| Descriptor and implementation drift | Validate registries and require strategies/roles in contract tests |
| Recipe migration loses future or unknown data | Central serializer, explicit versions, fixtures, and refusal of destructive saves |
| Controller split only redistributes coupling | Move ownership into project/application layers before dividing presentation classes |
| Premature plug-in design freezes the wrong ABI | Use statically registered strategies until an external plug-in need is demonstrated |
| V9938 drives shortcuts before foundations exist | Enforce framework exit gates before its native mode implementation |

## Immediate next work

1. Establish Stage 0 registry, applicability, and shared recipe fixtures.
2. Add stable ID values and TMS9918A/F18A display-mode descriptors.
3. Adapt current enum APIs at the registry boundary with no output change.
4. Add the portable-module Qt dependency check.
5. Re-run the complete platform matrix and record the milestone result.

## Documentation governance

[docs/README.md](docs/README.md) classifies authoritative specifications,
operating guides, research, and historical evidence. When implementation
changes a public contract, update the owning document in the same change.
Completed checklists are either removed or labeled historical; they must not
continue to present themselves as the active plan.
