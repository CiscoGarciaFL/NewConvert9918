# RetroVDP Studio architecture

Status: accepted target architecture, audited 2026-09-30.

This document defines the structure toward which the codebase evolves. It is
the authority for module boundaries, dependency direction, extension points,
and ownership of project data. The staged migration is in
[ARCHITECTURE_IMPLEMENTATION_PLAN.md](ARCHITECTURE_IMPLEMENTATION_PLAN.md).
Product-facing target and editor behavior remains in
[TARGET_PROFILES.md](TARGET_PROFILES.md),
[VDP_SUPPORT_ROADMAP.md](VDP_SUPPORT_ROADMAP.md), and
[VDP_DESIGN_TOOLS.md](VDP_DESIGN_TOOLS.md).

## Product architecture

RetroVDP Studio is an asset-production system for a campaign or software
project. A reusable source asset may be expressed through several target
hardware profiles. Each target applies its own display-mode rules, compiler,
validation, preview, and native export formats without overwriting another
target's result.

The durable workflow is:

```text
source library
    -> normalized raster or structured assets
        -> target hardware + display mode + compile settings
            -> managed target artifact
                -> hardware validation + target-faithful preview
                    -> native files or deployment-package manifest
```

The principal asset families are screen images, palettes, character or pattern
sets, tile/name maps, sprite pattern sets, sprite/object placements, animation
sequences, framebuffer pages, and display/register state. New target hardware
may support only a subset. Applicability is declared by descriptors; the UI
does not infer support from a target name.

## Audit of the current code

The present foundation is sound enough to evolve in place. A rewrite is not
justified.

### Strengths to preserve

- `retrovdp_core` uses standard C++ values and is independent of Qt and the
  operating system.
- Image bounds, row stride, cancellation, diagnostics, target tables, and
  generated-file manifests are explicit instead of implicit global state.
- Conversion algorithms are deterministic, independently testable, and backed
  by focused and golden fixtures.
- File generation is separated from filesystem writes, allowing complete
  collision preflight and atomic replacement.
- The GUI and CLI share the conversion path instead of maintaining separate
  conversion implementations.
- Domain classes are generally final values or cohesive services. There is no
  fragile target-class inheritance hierarchy to unwind.

### Structural risks

| Priority | Finding | Consequence |
| --- | --- | --- |
| P0 | `ConversionMode`, `TargetProfileId`, `TargetTableRole`, and `ExportFormat` are closed numeric enums used across registries, UI state, CLI parsing, recipes, and writers. | Every target or format expands central switches and numeric persistence becomes fragile. |
| P0 | Geometry, the fifteen-color working palette, and TMS9918A preparation assumptions are embedded in the application conversion pipeline. | A V9938 or non-TI target cannot be added as a descriptor-only extension. |
| P0 | `ImageInputController` owns source state, edits, settings, jobs, preview publication, export state, recipes, undo, and a large QML surface. | Changes have broad regression scope and application policy cannot be reused without Qt presentation state. |
| P0 | Character and sprite value types, histories, and JSON persistence are private to `EditorProjectController`. | The GUI is the domain model; the CLI, tests, future project browser, and target compilers cannot share editor assets cleanly. |
| P1 | `runConversion`, target-table layouts, export applicability, and several CLI tables are separate centralized dispatch points. | Registration is duplicated and can disagree as the target catalog grows. |
| P1 | The application support library links Qt image I/O and contains orchestration that is otherwise platform-neutral. | Headless workflows carry unnecessary Qt/presentation coupling. |
| P1 | GUI save and CLI recipe load each understand the JSON schema directly. | Schema changes require coordinated edits in unrelated frontends and risk partial compatibility. |
| P2 | Large implementation and test translation units concentrate unrelated behavior. | Reviews and incremental builds become slower, though size alone is not a reason to split algorithms. |

The audit found a boundary problem, not an inheritance problem. Adding a base
class per chip would make the design worse: F18A and V9958 are partial,
mode-specific compatibility relationships rather than honest “is-a”
relationships. The architecture therefore uses composition, descriptors, and
narrow strategies.

## Architectural principles

1. Sources are reusable and target outputs are independently owned.
2. Stable string identifiers are the persistence, CLI, and registry boundary.
3. Hardware facts are immutable typed descriptors, not UI conditionals.
4. Compilation, validation, preview, import, and export are distinct
   operations with independently testable contracts.
5. Built-in targets compose reusable stages; they do not inherit an entire
   older target implementation.
6. The domain and application layers use standard C++ types. Qt belongs in
   adapters and presentation.
7. Code that creates bytes does not open files. Code that writes files does not
   decide hardware layout.
8. A target artifact is validated before an exporter accepts it. Exporters do
   not silently repair invalid target data.
9. Project regeneration is explicit when it would replace manually edited
   target data.
10. Existing TMS9918A/F18A behavior and recipe IDs remain compatible throughout
    migration.
11. Extension seams are introduced from demonstrated variation. There is no
    dynamic plug-in ABI until an actual external plug-in requirement exists.
12. Each migration step leaves the application releasable.

## Dependency model

Dependencies point inward and downward only:

```text
Qt Quick / CLI frontends
          |
          v
presentation adapters ------ Qt/filesystem/clipboard adapters
          |                              |
          +--------------+---------------+
                         v
                application use cases
                         |
          +--------------+------------------+
          v              v                  v
    project/assets   target registry   format registry
          |              |                  |
          +--------------+------------------+
                         v
               portable domain values
```

Target implementations depend on portable domain values and reusable compiler
stages. The registries know target and format implementations. Domain values do
not know registries, Qt, filesystems, QML, command-line syntax, or concrete
targets.

## Modules and ownership

The names below describe responsibilities. They may begin as separate folders
inside an existing library and become CMake targets only when that improves
dependency enforcement or build time.

### `retrovdp_domain`

Owns small, stable standard-C++ values:

- validated `AssetId`, `TargetId`, `ModeId`, `FormatId`, and `RegionRoleId`;
- images, indexed pixels, palettes, rational pixel aspect, and dimensions;
- asset-kind and target-kind enums;
- diagnostics, result/status values, cancellation, and content hashes; and
- target artifacts and memory regions with explicit placement and alignment.

These values contain invariants but no workflow, registry lookup, Qt values,
or filesystem behavior.

### `retrovdp_assets`

Owns the editable project model:

- source assets and provenance;
- structured pattern, map, sprite, object, palette, and display-state assets;
- target configurations;
- managed outputs and stale/manual-edit state;
- allocation, reservation, and lock metadata; and
- project-level undoable commands or transactions.

The model is usable headlessly. QML never stores the authoritative copy of an
asset.

### `retrovdp_targets`

Owns hardware descriptions and target behavior registration:

- `TargetDescriptor` and `DisplayModeDescriptor`;
- typed palette, tile/map, framebuffer, sprite/object, layer, and memory
  capabilities;
- compiler, validator, and preview strategy registration;
- descriptor validation and duplicate-ID detection; and
- compatibility declarations between specific modes or components.

Summary capability flags may be derived for UI filtering, but are not an
independent source of truth.

### Target-family implementations

Target-family folders own codecs and reusable stages such as TI/Yamaha tables,
planar tiles, packed tiles, character allocation, object streams, and
framebuffer address mapping. Concrete targets register descriptors and compose
these stages.

The current bitmap, multicolor, and F18A algorithms remain focused functions.
They become strategies behind registrations; they do not need to become class
hierarchies.

### `retrovdp_formats`

Owns source-format and export-format descriptors plus pure codecs. Import and
export applicability is stated in terms of asset kinds, target/mode IDs, and
required region roles. Generated files are returned in an owned manifest.

Visual decoding and structure-preserving import are separate capabilities of a
format. A format can support one without supporting the other.

### `retrovdp_application`

Owns frontend-neutral use cases:

- open source and import structured asset;
- create/load/save a project through a persistence port;
- compile or regenerate a managed output;
- validate and render a target artifact;
- build and write an export/deployment manifest;
- coordinate cancellable generations and publish only current results; and
- report conflicts that require user choice.

Use cases accept ports for persistence, file selection, clipboard access, and
background execution. They return domain results and do not emit QML signals.

### Adapters and frontends

Qt adapters own `QImage`, `QJson*`, `QSaveFile`, clipboard, settings, file
dialogs, and thread-pool integration. The Qt presentation layer exposes small
workspace-specific view models and translates domain notifications to QML.
The CLI parses text, invokes the same application use cases, and renders text
or JSON results.

## Core contracts

### Identifiers

Persisted identities are validated strings such as `tms9918a`,
`graphics-ii`, `pattern`, and `ti-files`. Display names are localizable labels
and may change. Existing numeric enums may remain temporary implementation
details, but serializers and frontends resolve through the registries.

Unknown IDs are preserved where safe during project load and produce an
explicit unavailable-component diagnostic. They are never coerced to a nearby
enum value.

### Descriptors

A display-mode descriptor defines logical and visible geometry, rational pixel
aspect, palette model, pixel/tile encoding, layer and object rules, memory
regions, supported asset kinds, and registered strategies. Descriptors are
immutable after registry construction.

Descriptor validation runs at startup and in unit tests. Duplicate IDs,
missing strategies, contradictory capabilities, invalid sizes, and overlapping
fixed memory regions fail registration rather than surfacing during export.

### Compilation

A compile request contains a normalized asset snapshot, target and mode IDs,
typed settings, and cancellation/progress channels. It returns a target
artifact containing stable-ID memory regions, structured metadata,
diagnostics, and provenance. It does not write files.

Common preprocessing is selected by the compiler plan. Geometry and palette
limits come from the mode or compiler, never from application-global defaults.

### Validation and preview

Validation consumes the encoded artifact so it can detect actual allocation,
addressing, palette, scanline, collision, and capacity failures. A previewer
also consumes the encoded artifact and descriptor, producing what the target
would display, including pixel aspect and relevant layer/object limits.

A convenient compile operation may invoke all three steps, but their contracts
remain separate for direct editing and imported native data.

### Target artifacts and memory regions

The existing role-tagged byte-table idea remains. A region adds:

- a stable role ID;
- optional address, bank, page, or slot placement;
- alignment and size constraints;
- relationship to its target mode and asset;
- generated versus user-edited provenance; and
- optional structured metadata needed by validators or exporters.

This generalizes pattern/color tables to palette RAM, VRAM pages, tile maps,
object records, register state, masks, and runtime helper data.

### Format registrations

One registration answers:

- which signatures and extensions it recognizes;
- whether it can decode a visual source;
- which structured asset kinds it can import and under what constraints;
- which artifact roles it can export for which targets/modes; and
- its round-trip and information-loss guarantees.

PNG encoding may remain a Qt adapter while the registry exposes it as the same
logical format operation.

### Project persistence

The project file is a versioned envelope with stable IDs. Parsing, migration,
validation, and writing are centralized behind one project serializer used by
GUI and CLI. The in-memory project stays Qt-independent even if the first JSON
adapter uses Qt.

Saves are atomic. Load limits cover total bytes, asset counts, dimensions,
region sizes, nesting, and history. Older readers must not silently rewrite
unknown future fields. Schema migration is explicit and fixture-tested.

Each managed output records source identity/hash, target/mode IDs, compiler
settings, generated regions, diagnostics, preview state, export history, and
whether manual edits make regeneration destructive.

## Inheritance and polymorphism policy

- Prefer values, free algorithms, and composition for hardware facts.
- Do not model `F18A : TMS9918A` or `V9958 : V9938`. Register verified shared
  stages and explicit compatibility per mode.
- Use a narrow pure interface only for a real runtime substitution boundary,
  such as an injected project store or external execution service.
- Built-in compiler, validator, preview, importer, and exporter registrations
  should initially use typed function objects or references to stateless
  services. This keeps ownership clear and avoids speculative virtual APIs.
- If external binary plug-ins are later required, design a versioned C ABI or
  process protocol separately; C++ virtual classes are not a stable plug-in
  ABI.

## Presentation policy

Replace the two broad controllers incrementally with workspace-focused models:

- source preparation;
- project/session and selection;
- screen-image compile settings;
- character/pattern editing;
- sprite/object editing;
- diagnostics and memory summary; and
- export/deployment.

A presentation model may expose Qt properties, but it delegates mutations to
application use cases and holds only selection, transient tool, and view state.
Cross-workspace state lives in the project/session model.

QML receives descriptor-derived lists and capability state. It must not contain
target-name comparisons, persisted numeric target indexes, table sizes, or
hardware limits.

## Concurrency and failure policy

- Domain values passed to background work are immutable snapshots or shared
  immutable data.
- Compilers are reentrant unless a registration explicitly owns isolated
  state.
- Every long operation accepts cancellation and returns structured diagnostics.
- Generation filtering remains in the application layer; stale work may finish
  but cannot replace a newer result.
- Exceptions are reserved for programmer errors or unrecoverable invariant
  violations inside a boundary. User input, unsupported capability, malformed
  data, and I/O failures are ordinary result values.
- Progress callbacks never expose a mutable artifact owned by a worker.

## Repository direction

The intended organization is:

```text
include/retrovdp/
    domain/        portable value contracts
    assets/        project and editable asset contracts
    targets/       descriptors, registries, strategies
    formats/       format descriptors and pure codec contracts
    application/   frontend-neutral use cases
src/
    domain/
    assets/
    targets/common/
    targets/ti9918/
    targets/f18a/
    formats/
    application/
adapters/qt/       image, JSON, filesystem, clipboard, settings, jobs
app/gui/           Qt/QML presentation and desktop executable
app/cli/           command-line frontend
tests/
    unit/           values, descriptors, codecs, algorithms
    contract/       registries, schema, target/format contracts
    integration/    use cases and adapters
    golden/         approved byte and image fixtures
```

This layout is a destination, not permission for a mass move. Folders and
CMake targets are introduced only as the implementation plan reaches their
boundary.

## Enforcement

Architecture is complete only when it is executable through tests and build
boundaries:

- portable targets compile without Qt include paths or Qt link libraries;
- registry contract tests enumerate every descriptor and reject duplicates;
- GUI and CLI recipe fixtures pass through the same serializer;
- every implemented target/mode registers compiler, validator, and preview
  behavior;
- every exporter declares and tests applicability from artifact roles;
- existing golden outputs remain unchanged unless an intentional difference is
  reviewed and recorded; and
- source or target additions do not require edits to unrelated frontends.

## Accepted tradeoffs

- Stable IDs add lookup and validation compared with compact enums, but are
  necessary for durable projects and an expanding target catalog.
- Registries add indirection, but remove duplicated global switches and make
  descriptors testable as a complete set.
- Some Qt remains in the first persistence and image adapters. The important
  boundary is that Qt does not own project or target meaning.
- The architecture favors built-in, statically linked target modules now. A
  binary plug-in system would add compatibility and security obligations before
  there is a demonstrated distribution need.
- Existing converters keep specialized optimized code. Reuse is pursued for
  validated stages and contracts, not by forcing dissimilar hardware through
  one universal algorithm.
