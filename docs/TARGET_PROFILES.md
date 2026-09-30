# Target profiles and project outputs

## Product model

RetroVDP Studio is a multi-target graphics workspace. A VDP name is not part
of the product identity and a source file is not permanently tied to the
hardware or application that created it.

The durable workflow is:

```text
source library
    -> normalized visual source or compatible native assets
        -> one or more target profiles
            -> independently managed target outputs
                -> target-compatible exports
```

TMS9918A and F18A are the implemented target profiles. V9938 is the next
planned profile. Additional profiles must be registered through the same core
contract rather than added as unrelated UI modes.

## Source formats are target-independent

Every safe, documented retro format that RetroVDP Studio can decode should be
eligible for **Open as Source**. The decoder renders the file into a normalized
visual representation, retains useful provenance and palette metadata, and
allows Conversion to apply the selected target's rules. A file's historical
chipset or software association is descriptive metadata, not an artificial
import restriction.

This visual route may intentionally discard native organization such as tile
indexes, sprite attributes, or pattern reuse. The result is new target data
derived from the rendered appearance.

## Native editor imports are capability-gated

**Import into Editor** is separate from Open as Source. It is offered only when
the decoded asset kind and active target can represent the source structure in
a meaningful way. Native imports may preserve:

- character and pattern definitions;
- sprite patterns and sprite attributes;
- tile or name maps;
- palettes and color attributes;
- animation frames or display lists; and
- target memory tables when their layout is explicitly known.

Every native importer declares whether the operation is lossless, constrained,
or converted. When direct import is not valid, the file remains available as a
visual source instead of disappearing from the application.

## Target-profile contract

Each target profile has a stable identifier and declares capabilities rather
than relying on target-name checks throughout the application. The contract
includes:

- implemented and planned status;
- display modes and geometry;
- VRAM size and memory-table layouts;
- fixed or programmable palette rules;
- pattern, tile-map, layer, and attribute capabilities;
- sprite sizes, color depths, limits, priority, and scanline behavior;
- compatible conversion modes;
- native asset import capabilities;
- supported export formats; and
- compatibility relationships that have been explicitly verified.

A newer target must not be treated as a complete superset merely because it
inherits part of an earlier VDP's behavior.

## Current workspace and future projects

The current application works with Screen Image, Character Editor, and Sprite
Editor documents individually. Existing recipes retain that behavior. This is
not yet the complete multi-target project model.

The future project model will own:

- a shared source library;
- a selected set of target profiles;
- one managed output per target and workspace asset;
- the settings and generated data used by each output;
- warnings produced by the target profile;
- explicit synchronization or regeneration state; and
- independent target export history.

Changing one target's settings or manually editing its output must not silently
replace another target's output. Shared source changes may mark managed outputs
stale, but regeneration remains an explicit, reviewable operation when it could
replace edited target data.

## File-format registry

Format support should be registered independently from target profiles. Each
format entry records:

- stable format identifier and recognized extensions/signatures;
- originating software and hardware ecosystems;
- whether it can be opened as a visual source;
- native asset kinds it can contain;
- editor importers and their compatibility requirements;
- assumptions required for headerless or ambiguous data;
- applicable exporters and target profiles; and
- round-trip guarantees and known information loss.

This separation allows RetroVDP Studio to accept a growing library of historic
formats while keeping direct editor operations honest about hardware meaning.

## Interface language

The interface uses **Target VDP** for the active hardware rules and **Target
mode** for a display or conversion mode within that profile. File commands use
**Open as Source** for the broad visual path and asset-specific verbs such as
**Import Patterns**, **Import Sprites**, **Import Tile Map**, and **Import
Palette** for structure-preserving operations.

Supported targets belong in project configuration, status displays, and About
details—not in the RetroVDP Studio product name.
