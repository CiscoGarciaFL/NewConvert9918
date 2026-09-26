# 9918 and F18A Design Tools

## Status and intent

This document specifies a future application feature set. It does not describe
functionality available in the current application and does not authorize
implementation yet.

The design-tools workspace will complement image conversion with hardware-aware
authoring tools for:

- TMS9918A and F18A sprites;
- arbitrarily arranged composite sprites;
- one, two, or three visible character-pattern banks;
- individual character/pattern editing;
- character allocation and reservation for image construction; and
- an accurate TMS9918A Multicolor display simulation.

## Source-image preparation tools

The first source-screen extension is a color-control foundation for preparing an
image before it is sent to Screen Image or imported into an editor mode. The
existing background-color control becomes a paired foreground/background
control in the same compact footprint. The two swatches overlap diagonally:
foreground is top-left and background is bottom-right. Selecting either swatch
brings it to the front, makes it the active color, and opens the color picker.

The picker has three tabs:

- **Spectrum** — a deterministic PC/VGA-style RGB palette with 16, 32, or 64
  entries. The 16-color view is the standard base palette; 32 adds one tone
  shift per base color, and 64 adds darker, lighter, and midpoint companions.
  Black and white are always the first two entries.
- **Standard** — a compact general-purpose color swatch for colors outside the
  source's most common colors.
- **Used by image** — unique source colors in a scrollable grid. Neutral colors
  run from black to white, followed by chromatic colors in hue order. Fully
  transparent pixels are excluded. The UI displays at most 4,096 colors,
  sampled evenly from the ordered set when a photograph contains more, to keep
  very large images responsive.

The source eyedropper applies to whichever swatch is active. The new active
color path preserves the selected RGB value, while the legacy background-pick
API remains palette-constrained for compatibility with existing conversion
automation. Foreground is retained as an independent editing color.

The source pane now has an upper drawing ribbon. Its first tools are compact
**Pencil**, **Eraser**, **Ellipse**, and **Rectangle** icon buttons, plus a dot
and narrow 1–64 pixel diameter editor. Pencil paints with the foreground color;
Eraser paints with the background color. Ellipse and Rectangle draw foreground-
color outlines using the selected diameter. Dragging sets independent width and
height; holding Shift during the drag locks them to equal displayed dimensions,
producing a circle or square. Drawing edges default to a soft, one-pixel
coverage blend, while the **Hard Edge** toggle makes every covered source pixel
a full foreground/background overwrite. The shape-fill toggle switches between
no fill and the current background color. Filled shapes draw the outline first,
then overlap the fill into the outline's interior antialias fringe so no blended
halo or ring remains between the trace and fill. Strokes and shapes are
stored on an RGBA edit layer at the prepared 256×192 resolution and composited
after source framing. This keeps hard strokes and fills fully opaque instead of
resampling them together with the original image. The same composed canvas is
the conversion input. Fit, crop, source offsets, and scaling-filter changes
rebuild the underlying frame while retaining the edit layer; drawing into
PowerPaint padding has no effect. The Screen Image preview is scheduled after a
stroke ends, while the source preview retains its current frame. A lightweight
preview overlay draws every pointer segment immediately;
the edited source preview replaces it when the mouse is released. While Pencil
or Eraser is active, a high-contrast ring follows the crosshair and shows the
zoom-scaled diameter that will be applied. The destination pane reserves the
same upper-ribbon height as the Source pane so both sides stay balanced and
mode-specific tools have an established location.
Undo and redo buttons treat each completed drawing stroke as one transaction.
The history stores prepared-canvas edit-layer states rather than
pencil-specific commands so future drawing tools can participate in the same
undo/redo path. Starting a new
drawing change after an undo clears the redo branch. Ctrl+Z prefers drawing undo
when drawing history is available, and Ctrl+Y redoes a drawing change.

The **Automatically update conversion** switch belongs to the Screen Image
pane's lower tool area because it controls when that output is regenerated, rather
than how the source image is prepared.

## Workspace modes and Side Panel

The main window keeps Source as a common preparation and reference pane. The
right-hand destination pane is selected through a top-level **Mode** menu with
three mutually exclusive checked entries:

1. **Screen Image** — the existing complete-image conversion preview;
2. **Character Editor** — direct TMS9918A/F18A character-pattern editing; and
3. **Sprite Editor** — direct TMS9918A/F18A sprite-pattern editing.

The old Converted title becomes **Screen Image**. In tabbed layout, the second
tab follows the selected mode; in split layouts, the right pane changes its
title and content. Switching modes is non-destructive: each destination retains
its document, selection, tools, undo history, and viewport state. Source remains
available in every mode and does not get replaced by an editor document.

All three destination workspaces share the same visual shell: the same title
treatment, an upper tool-ribbon area, a bordered canvas/preview viewport, and a
lower tool row with the standard zoom control. Character and Sprite controls
will populate those reserved areas as their editors are implemented; zoom stays
disabled until a mode has content that can be displayed.

The former Conversion Panel is user-facing as the **Side Panel**. View exposes
**Show Side Panel** and **Side Panel Placement → Adjacent / Overlay**. Its
contents follow the workspace mode: Screen Image presents **Image Settings**,
Character Editor presents **Character Options**, and Sprite Editor presents
**Sprite Options**. Commands
that are not meaningful for the active mode, such as exporting a screen image
while an editor is active, are disabled rather than acting on hidden state.

Character and sprite acquisition from Source is an explicit **Import from
Source** operation. Its Side Panel workflow selects a source region and previews
scaling, target dimensions, color reduction, palette fitting, and—where
applicable—transparency before committing editable hardware data. Subsequent
source changes must not silently overwrite committed character or sprite work.

This remains a focused preparation layer rather than a general raster editor.
Foreground/background colors now drive pencil, eraser, shape outlines, and the
optional shape fill; later discussion should decide how flood fill,
replace-color, and selection tools participate.

These are not generic pixel-art tools. Every editor and preview must understand
the selected video chip, display mode, memory-table organization, palette, and
resource limits. The workspace must show when artwork is visually attractive
but impossible to reproduce on the selected hardware.

## Product goals

1. Make the TMS9918A's limits visible while the user draws, rather than only
   reporting them during export.
2. Expose F18A enhancements without confusing them with output that will run on
   an original TMS9918A.
3. Allow several small hardware sprites to be designed and positioned as one
   larger, irregular object.
4. Make pattern-bank use, reuse, conflicts, and reserved character ranges easy
   to understand.
5. Let a Screen Image result become editable hardware data instead of remaining
   a final opaque conversion result.
6. Reuse the project's palettes, target-memory types, validation, preview, and
   export infrastructure without coupling the design model to QML.
7. Produce deterministic project files and target tables on Windows, Linux,
   Intel macOS, and Apple Silicon macOS.

## Terminology used by the application

Historical documentation and software do not always use the mode names in the
same way. The interface should show an explicit hardware-oriented name and may
include a familiar alias:

| Application label | Meaning in this document |
| --- | --- |
| Graphics I — one pattern bank | A 32x24 tile display using one set of 256 8x8 patterns; colors are shared by groups of patterns. |
| Graphics II / Bitmap — three pattern banks | A 32x24 display whose top, middle, and bottom eight tile rows address separate 256-pattern regions and corresponding color data. |
| Multicolor — 64x48 logical pixels | A 256x192 display made from 4x4-pixel color cells encoded through the pattern/name-table organization. |
| Character or pattern | One indexed 8x8 definition. “Character” emphasizes text; “pattern” also covers artwork tiles. |
| Pattern bank | One index space containing as many as 256 8x8 definitions. |
| Sprite pattern | An 8x8 bit pattern in the sprite-generator table; larger sprites consume a hardware-defined group of patterns. |
| Sprite instance | A positioned sprite-attribute entry that refers to sprite pattern data. |
| Composite sprite | A design-time object containing multiple arbitrarily positioned sprite instances. |

The bank viewer can display one, two, or three banks simultaneously for design
and comparison. Export validation still follows the selected hardware mode; a
two-bank view is not presented as a new TMS9918A display mode.

## Hardware profiles

Every design project chooses a target profile. Switching profiles runs a
non-destructive compatibility analysis before it changes the active target.
The profile is a shared document setting, not a separate Screen Image,
Character, or Sprite workspace. A single **Target Hardware** selector appears
consistently in each mode's Side Panel, while the canvas keeps the same artwork
and rerenders it using the selected chip and enabled capabilities. Mode-specific
options expose only valid combinations. Switching from F18A to TMS9918A must
preview and report required reductions before committing; it must not silently
discard enhanced color, attributes, layers, or sprite data.

### Original TMS9918A profile

The initial profile models these constraints:

- 32 sprite-attribute entries in one display list;
- no more than four sprites displayed on one horizontal scanline;
- 8x8 or 16x16 sprite size selected globally;
- optional 2x sprite magnification selected globally;
- one visible color plus transparency per standard sprite instance;
- sprite priority, collision, early-clock/offscreen behavior, and end-of-list
  behavior;
- the original fixed palette and transparent color semantics;
- 256 pattern indexes in a single Graphics I bank;
- three 256-pattern regions for a conventional Graphics II/Bitmap display; and
- standard 4x4 logical pixels in Multicolor mode.

The project may offer machine presets such as TI-99/4A, ColecoVision, MSX1, or
Adam later, but the chip-level model should remain independent of host-machine
file formats and address conventions.

### F18A enhanced profile

The F18A profile begins with TMS9918A compatibility and can selectively enable:

- as many as 32 displayed sprites on one scanline;
- per-sprite 8x8 or 16x16 size;
- per-sprite horizontal and vertical pattern flip;
- enhanced color modes with 1, 2, or 3 bits per pixel, providing 2, 4, or 8
  color indexes per tile or sprite;
- palette selection from 64 programmable 12-bit color registers (64 loaded
  colors selected from 4096 possible RGB values);
- per-tile attributes, transparency, flipping, and sprite/tile priority; and
- a second tile layer when the project explicitly targets an enhanced layout.

Sprite linking is not part of the current F18A target contract; it was removed
from the released V1.9 firmware and must not be offered as an exportable F18A
feature.

F18A capabilities must be individual project flags rather than one vague
“enhanced” switch. The preview and exporter need to know exactly which features
are being used.

### Custom diagnostic profile

A custom profile may let advanced users lower a budget—such as limiting a line
to three sprites—to test conservative game-engine requirements. It must never
allow a value beyond the actual selected hardware and then describe the output
as compatible.

## Proposed workspace

The initial design tools are integrated into the main Source/destination layout
through the **Mode** menu. Screen Image, Character Editor, and Sprite Editor
share the source preparation pane while providing independent destination
documents and contextual Side Panels. More extensive project-level views such
as tile maps, Multicolor scenes, memory allocation, and composite-sprite scene
management may later expand from these editor modes without turning the Side
Panel itself into the editing canvas.

A common workspace layout can contain:

- an asset navigator for sprites, composites, pattern banks, tile maps, and
  Multicolor scenes;
- a central zoomable pixel or scene canvas;
- a palette and drawing-tool strip;
- a target/mode inspector;
- a hardware budget and diagnostics panel; and
- a memory-map/export summary.

The existing compact zoom controls, theme-aware frames, status bar, and
keyboard accessibility should be reused. Pixel editors need an optional grid,
integer zoom, pan, coordinates, and a visible hardware-pixel boundary.

The initial foundation now presents the dual output profiles in every Side
Panel, three persistent 256-slot Character sets, repeatable 32-slot Sprite
sets, and a Sprite placement workspace whose box size and all 32 coordinates
are project state. Sprite sets are suitable for alternate images or animation
frames. Pixel extraction and hardware-table generation attach to these models
later; disabled import commands explicitly indicate that boundary rather than
claiming that placeholder data was generated.

The broader project model may eventually expose these major pages:

1. **Sprites**
2. **Characters**
3. **Tile/Image Layout**
4. **Multicolor**
5. **Memory and Export**

They share one project model, so a sprite can be previewed over a tile scene and
an edited character immediately updates every name-table position that uses it.

## Sprite design tools

### Sprite sets and placement workspace

A sprite set contains 32 indexed 8x8 definitions, 32 indexed 16x16 definitions,
and exactly 32 positioned sprite instances matching one complete hardware
attribute list. Both definition banks remain visible and editable. A project may
contain multiple sets for animation frames or alternate images. Each set retains
its own patterns and placements, while shared import and target-profile recipes
remain reusable across sets.

The placement workspace is a bounded, configurable box with the current Screen
Image as an optional reference underlay. All 32 sprite instances may overlap and
be dragged independently at one-pixel resolution, including partially offscreen
positions. TMS9918A placement uses the globally selected 8x8 or 16x16 bank;
F18A placement uses each sprite instance's selected size. A future **Populate
from Screen Image** operation will sample the artwork covered by each placed
sprite and generate the corresponding patterns under the active TMS9918A and
F18A rules. It must preserve slot order, report scanline overflow and color
conflicts, and preview changes before replacing existing sprite data.

### Sprite-pattern editor

The first implemented sprite editor provides:

- one active 8x8 or 16x16 editing canvas above stacked 32-entry banks;
- pencil, eraser, translate/Pan, horizontal mirror, vertical flip, clockwise
  rotate, clear, grouped undo/redo, and validated JSON copy/paste;
- transparent and active-color selection;
- a shared one-bit TMS9918A baseline with one visible instance color;
- non-destructive F18A overrides with independently selected size and 1-, 2-,
  or 3-bpp indexed pixel values;
- a compact hardware preview and thumbnail library with hexadecimal indexes;
- import from a monochrome or indexed image with an explicit threshold/palette
  mapping preview (planned); and
- row-mask inspection for users who work directly with VDP data.

Line, shape, flood-fill, selection, names, usage counts, and compatibility badges
remain later extensions; their data model must build on the same two banks and
baseline/override contract rather than creating a second sprite representation.

For original 16x16 sprites, the UI can present one continuous 16x16 canvas, but
the memory inspector must show the four underlying 8x8 pattern definitions and
their required alignment/order. It must not let convenient drawing hide an
invalid pattern index.

### Sprite instances and attribute list

Pattern data and positioned instances are different resources. The instance
editor must expose:

- sprite-list index and hardware priority;
- X and Y position, including partially offscreen placement;
- pattern reference;
- color or F18A palette/ECM attributes;
- effective size and magnification;
- X/Y flip where supported;
- visibility and design-only labels; and
- optional anchor contribution to a composite.

Reordering the hardware list can change both priority and which sprites survive
a scanline limit, so drag-to-reorder must update the accurate preview
immediately.

### Arbitrary composite sprites

A composite is not restricted to a rectangular grid. It is a group with:

- a named origin/anchor;
- any number of child sprite instances up to the hardware/project budget;
- signed X/Y offsets from the anchor;
- gaps, overlaps, negative offsets, and irregular silhouettes;
- optional subgroups for pieces such as body, weapon, shadow, or status marker;
- a stable internal draw/list order; and
- a bounding box calculated from children, not imposed on their layout.

The canvas should support moving the whole composite, moving individual
children, alignment/distribution commands, snapping to pixels or sprite-sized
increments, and temporarily locking children.

The design-time group remains useful on both chips even when it has no direct
hardware representation. Export can produce the pattern data, initial
attribute entries, and a manifest of child offsets for application code. The
current F18A target does not provide sprite linking, so groups remain a
design-time/application-code construct.

### Overlay and color-effect preview

The editor needs two distinct overlap views:

1. **Hardware preview** — applies actual transparency, priority, collision, and
   sprite-per-line behavior. Original TMS9918A sprites do not alpha-blend;
   overlapping opaque pixels cause priority/collision behavior rather than a
   newly mixed color.
2. **Design analysis overlay** — highlights opaque-pixel intersections,
   coverage contributed by each child, and areas removed by priority or line
   overflow.

Multiple differently colored monochrome sprites can form a multicolor-looking
composite when their opaque pixels occupy different locations. The analysis
view should help construct that effect without falsely simulating additive or
alpha color mixing. F18A enhanced-color sprites use their selected ECM and
palette rules in the hardware preview.

### Scanline and sprite-budget analyzer

Every sprite scene and composite displays a 192-line budget graph:

- count of intersecting sprite instances on every scanline;
- target limit and optional project limit;
- green below the limit, warning at the limit, and error above the limit;
- the exact sprite-list entries suppressed on each overflowing TMS9918A line;
- opaque-pixel collision locations separately from rectangle overlap;
- total attribute entries used out of 32; and
- pattern-generator bytes and unique patterns consumed.

Selecting a line in the graph highlights the participating sprites. Selecting
an overflow diagnostic highlights both the four that remain and every later
entry that becomes unavailable on that line. F18A mode changes the line budget
to its enabled enhanced limit while preserving the option to preview original
four-per-line compatibility.

Animation-aware worst-case analysis is desirable later, but static composite
and scene analysis is the first requirement.

## Character-set viewer

### Bank presentation

The character viewer provides:

- a 16x16 index grid for one 256-pattern bank;
- one-, two-, or three-bank side-by-side and stacked layouts;
- synchronized selection and zoom across visible banks;
- decimal and hexadecimal pattern indexes;
- usage count, reservation state, tags, and a thumbnail for each pattern;
- filters for used, unused, duplicate, reserved, locked, or conflicting
  patterns; and
- difference highlighting for the same numeric index across multiple banks.

The standard editor keeps three 256-pattern sets available together. This maps
naturally to the three Graphics II screen regions while remaining useful as
three independent banks in other workflows. A future screen-to-sets command can
allocate a converted Screen Image across all three sets; the inverse command
can assemble selected sets back into an image without changing their indexes.

For Graphics II, the viewer labels banks by their normal screen region:

- Bank 0: top eight character rows;
- Bank 1: middle eight character rows; and
- Bank 2: bottom eight character rows.

A numeric pattern index is not a complete Graphics II identity by itself. The
UI and project model use `(bank, index)` wherever ambiguity is possible.

### Character/pattern editor

Selecting an entry opens an 8x8 editor with the common pixel tools. Its color
controls adapt to the mode:

- monochrome/global-color pattern editing where color is external;
- Graphics I group-color editing with a warning that changing the shared color
  entry affects the corresponding group of patterns;
- Graphics II row foreground/background editing;
- Multicolor nibble/cell editing; and
- F18A ECM pixel-index and palette editing.

The implemented tray hosts one or more reusable 8x8 Graphics II pattern
editors, each bound to an explicit `(set, index)`. Editors wrap into columns as
the pane width changes and the tray grows until it needs its own scrolling.
The inverted label identifies the active editor: selecting a pattern slot loads
that pattern into the active editor. The bottom-right plus/minus controls add an
active empty editor or remove the active editor while always preserving at
least one. An editor label's local menu can move it left, right, first, or last,
or remove it. Its pane-level upper toolbar provides pencil and eraser tools plus
overlapping foreground/background selectors, using the fixed TMS9918A hardware
palette for baseline editing and the current programmable 16-color palette for
F18A editing. Each row shows its
exact pattern byte on the left and foreground/background color byte on the
right. The vertical four-pixel boundary is emphasized, and the selected
pattern number is shown in hexadecimal below the grid. Edits update the
project’s real eight pattern bytes and eight row-color bytes and are persisted
in versioned recipes. Each editor also renders a compact character preview
under its color-byte column. Stacked plus/minus controls cycle exact 1x through
4x hardware-pixel sizes; the preview frame expands up and left from a fixed
bottom-right corner. Vertical Bits and Color captions flank the byte columns so
the grid can sit at the editor's top margin without moving the pattern label.

One Character toolbar button switches the shared tray between **Pattern Editor**
and **Tiling Screen** views. Its overlapping icon follows the foreground/background
color control convention: the current presentation's icon is brought to the
front, and its tooltip names the presentation that clicking will open. This is a
second presentation of the same editor slots rather than a separate list:
the active editor and active tile are the same selection, and the existing
plus/minus controls add or remove the same slot in either presentation. Tiling
uses a 256x192 screen canvas with Home at coordinate `(0,0)` in its upper-left
corner. Every tile has its true 8x8 boundary and drags in 8-pixel hardware-cell
steps through the 32x24 grid. A pattern label appears only while its tile is
hovered. Multiple slots may reference the same `(set, pattern)`; those tiles
highlight together when selected while retaining independent screen
coordinates. To place another instance, use `+` to create an empty tile and then
choose the same pattern from the pattern grid; the new tile shares the pattern
data but has its own screen position. Tiling retains every instance, while the
Pattern Editor presentation filters loaded duplicates by `(set, pattern)` so
each unique pattern has only one editor. Unassigned empty slots remain visible
until a pattern is chosen. Tile positions and the active presentation are saved
in recipes.
While Tiling is active, the destination zoom control scales the complete screen
grid and its tiles. The tray grows with the scaled 192-pixel screen height when
workspace space is available, retains the live window width, and scrolls when
the scaled canvas exceeds the available viewport. Clicking the combined mode
button returns directly to the Pattern Editor presentation.

The Character upper toolbar also provides clockwise Rotate, horizontal Mirror,
vertical Flip, and Blank operations for the active pattern. Blank clears the
bitmap while preserving its row-color bytes. Drawing strokes and these pattern
operations participate in Character undo/redo history.

Copy and Paste use the operating-system clipboard rather than an internal-only
slot. Copy publishes indented, versioned JSON with the format identifier
`newconvert9918.character-pattern`, an 8x8 size declaration, source set/pattern
metadata, eight hexadecimal bitmap-byte strings, and eight hexadecimal
row-color-byte strings. The text is intentionally readable in an ordinary text
editor and usable by scripts. Paste accepts this format (including numeric byte
values for script convenience), validates every byte and dimension, replaces
the active pattern, and records the replacement as one undoable edit.

The Character lower toolbar provides undo/redo and a toggleable Pan workspace.
Pan places the active 8x8 pattern at the center of a temporary 24x24 virtual
grid. Direction and center controls match the Source positioning controls, with
movement limited to eight pixels in each direction. Pixels clipped from the
visible center 8x8 remain available while Pan is active, so moving back is
non-destructive. Turning Pan off commits the final center viewport, resets the
virtual position to `(0,0)`, and records the complete pan session as one undo
step rather than one step per nudge.
The planned Sprite placement workflow should follow the same hardware-grid
interaction model with sprite-specific dimensions and chipset limits.

The editor also provides:

- previous/next used pattern and previous/next index navigation;
- duplicate, swap, move, and copy-between-bank operations;
- shift with wrap or transparent fill;
- live previews of every tile-map location affected by the edit;
- undo/redo at the project level; and
- a byte view showing the exact pattern and color data that will be exported.

Editing a pattern used in many positions is intentionally a linked edit. A
“make unique here” command may duplicate and reassign it if a free compatible
slot exists.

## Character allocation and reservation

### Slot states

Every `(bank, index)` has one explicit state:

- **Free** — available to an allocator.
- **Allocated** — contains a definition used by the current project.
- **Reserved** — unavailable to automatic allocation, whether currently empty
  or supplied later by a program.
- **Locked** — contains data that can be used but not automatically moved or
  replaced.
- **Conflict** — two requested resources require incompatible data or
  attributes at the same location.

Reservations can be individual indexes, contiguous ranges, named groups, or
the same numeric range across selected banks. Examples include font glyphs,
game engine markers, animation frames, UI tiles, or host-program data loaded
at runtime.

### Reservation manager

The manager shows:

- a visual memory map for every bank;
- free, allocated, reserved, locked, and conflicting totals;
- range names and notes;
- whether a reservation applies to one bank or all three;
- required alignment for sprite and enhanced definitions;
- import/export of a reservation manifest; and
- a dry-run report before an allocator moves or replaces anything.

Automatic compaction or deduplication must never move locked entries. It must
not move ordinary allocated entries without previewing the complete remap and
updating every name-table or sprite reference atomically.

### Allocating an image into character patterns

An image-to-pattern operation should:

1. frame or crop the source to a tile layout;
2. divide it into 8x8 tiles under the selected mode's color rules;
3. identify byte-identical reusable patterns and compatible color data;
4. allocate unique definitions around reservations and locks;
5. create or update the name table;
6. report capacity, conflicts, and quality losses before committing; and
7. retain the allocation manifest so a later re-conversion can preserve stable
   indexes where possible.

This is different from the current full-screen Graphics II converter. A
conventional 256x192 bitmap can consume all 768 position-specific pattern
definitions and all corresponding color rows. Reserving definitions while
retaining an arbitrary full-screen bitmap may therefore require leaving those
screen tiles untouched, reusing/deduplicating tiles, accepting a constrained
region, or using a different tiled conversion workflow. The interface must
explain the tradeoff rather than implying that reserved capacity is free.

Graphics I allocation also has a color-group constraint: moving a pattern to a
new index group can change which foreground/background color entry applies.
The allocator must treat compatible pattern and color placement as one problem,
not move bitmap bytes independently.

### Stable re-conversion

For game projects, changing the source image should not arbitrarily renumber
every pattern. The allocator should prefer, in order:

1. keeping an unchanged pattern at its existing index;
2. replacing a changed pattern in its prior unlocked slot;
3. reusing an identical compatible definition already present;
4. taking a free slot; and
5. reporting a conflict instead of silently overwriting a reservation.

An explicit “repack for minimum usage” operation may generate a broader remap,
but it remains separate from normal updates.

## Tile and image-layout view

The layout view connects pattern definitions to their on-screen use:

- a 32x24 character grid for the standard 256x192 display;
- optional F18A 30-row and expanded-map layouts when selected;
- paint by pattern, stamp, rectangle, fill, select, move, and copy operations;
- bank/region boundaries at character rows 8 and 16 in Graphics II;
- optional pattern index, color entry, reservation, and priority overlays;
- click-through from a tile to its pattern/color editor;
- highlighting of all positions using the selected pattern; and
- a hardware preview including enabled sprite layers.

The user can import the current New Convert result into a new design project,
or send a selected design canvas to the ordinary conversion workflow. Neither
operation silently modifies the other workspace.

## Multicolor image simulator and editor

The Multicolor page renders the actual 256x192 output while exposing its 64x48
grid of 4x4 logical color cells.

Required views are:

- **Pixel view** — edit logical cells directly with the active hardware
  palette.
- **Pattern view** — show the underlying character boundaries, pattern bytes,
  and high/low color nibbles.
- **Name-table view** — show which pattern definition supplies each location
  and reveal repeated-definition side effects.
- **Hardware composite view** — add the current sprite scene using the selected
  chip's priority and scanline limits.

The page supports drawing tools, palette replacement, import through the
existing Multicolor converter, direct table import, and PNG/reference-image
underlay. Editing a reused pattern must update all of its uses immediately;
“make unique here” uses the character allocator.

The simulator must distinguish the original Multicolor display mode from New
Convert's Dual Multicolor and Half Multicolor conversion techniques. Those
conversion techniques may be imported or previewed later, but they are not
described as a different native TMS9918A Multicolor memory mode.

## Shared project model and recipes

The core model should use standard C++ value types and remain independent of
Qt/QML presentation. A design project needs, conceptually:

- project metadata and format version;
- target hardware profile and enabled capabilities;
- palettes;
- pattern banks and color attributes;
- name/tile maps;
- sprite patterns and sprite instances;
- named composite sprites;
- Multicolor canvases/scenes;
- allocation/reservation/lock metadata;
- source-image links and import settings;
- export presets; and
- diagnostic suppressions with an explanation.

The persisted contract is a versioned, human-readable
`*.nc9918.json` recipe. It stores the complete Screen Image conversion settings,
source linkage, mandatory TMS9918A and optional inherited F18A profiles, preview
and edit scope, active Character and Sprite selections, three Character-set
descriptors, repeatable Sprite-set descriptors, placement-box dimensions, and
all Sprite coordinates. Sprite-set data now includes sparse 8x8 and 16x16
baseline pixels, sparse F18A override pixels, per-instance size/color/depth and
future-facing palette/flip attributes. GUI saves are atomic. The CLI consumes the same recipe
for Screen Image jobs; Character and Sprite recipes are rejected clearly until
their converters can emit hardware data.

Later project files will add allocation metadata and optional edit-history
recovery to this contract. Unknown future fields must not cause silent data loss
during an older-version save.

Autosave and recovery use a separate temporary/recovery artifact. Saving must
be atomic and must not overwrite imported source tables unless the user chooses
an explicit export destination.

## Import and export

### Imports

The workspace should eventually accept:

- pattern, color, name, sprite-generator, and sprite-attribute raw tables;
- applicable TIFILES and V9T9 wrappers;
- target tables from the application's current conversion result;
- ordinary raster images for tracing or conversion;
- F18A palette and enhanced attribute data; and
- a design-project reservation/allocation manifest.

Every import requires a preview of inferred table role, byte length, target
profile, destination bank/range, and overwrite conflicts. Ambiguous raw files
must not be guessed into a live project without confirmation.

### Exports

Exports should include, as applicable:

- pattern-generator, color, and name tables;
- sprite-generator and sprite-attribute tables;
- F18A palette and enhanced-attribute tables;
- raw, TIFILES, and V9T9 containers already supported by the project where the
  table contract applies;
- a PNG hardware preview and optional analysis overlay;
- a machine-readable manifest of banks, ranges, composite offsets, palettes,
  and generated files; and
- source-language data declarations through separately defined target dialects
  in a later phase.

The export preflight must show every generated file and collision before
writing, then use the existing atomic-write policy. Importing an exported table
set into an empty project should reproduce the same hardware preview wherever
the format contains all required state.

## Validation and diagnostics

Diagnostics are persistent, selectable, and tied to the affected resource.
They include:

- sprite-list use above 32 entries;
- scanlines above the active sprite limit;
- sprites or patterns using an unsupported size, flip, ECM, or palette feature;
- opaque sprite-pixel collisions;
- invalid 16x16 pattern alignment or incomplete pattern groups;
- reserved/locked pattern conflicts;
- pattern-bank overflow;
- Graphics I color-group incompatibility;
- Graphics II resources assigned to the wrong screen region;
- unsupported F18A features when switching to TMS9918A;
- tile-map references to undefined patterns; and
- exports missing tables required to reconstruct the selected scene.

Warnings may describe a deliberate effect; errors block incompatible export.
The hardware preview should have a **Show as hardware would display it** toggle
that remains available even while design-analysis overlays are visible.

## Undo, clipboard, and editing safety

- All drawing, allocation, reordering, and property changes participate in one
  bounded project undo/redo history.
- A compound operation such as repacking patterns or importing a composite is
  one undo step.
- Clipboard payloads contain both a portable image representation and a
  versioned application-specific representation when possible.
- Pasting incompatible F18A content into a TMS9918A project opens a conversion
  preview rather than discarding colors or attributes silently.
- Closing or changing projects prompts for unsaved changes independently from
  the main conversion source.

## Relationship to current conversion features

Screen Image mode remains optimized for turning a raster image into a complete
target image. Character Editor and Sprite Editor add direct hardware-data
authoring and resource management while retaining Source as an import/reference
surface.

The integration boundary should provide explicit commands:

- **Create Design Project from Screen Image Result**
- **Import Current Working Palette**
- **Send Design Preview to Screen Image**
- **Replace Selected Pattern from Image Selection**
- **Re-convert Allocated Image Using Existing Reservations**

The last command requires a new allocation-aware conversion path and should not
be approximated by overwriting current full-screen tables. All modes must
call shared palette, validation, table-layout, and image operations rather than
maintaining subtly different hardware rules.

## Proposed implementation phases

### Design Phase 0 — Hardware contracts and fixtures

- Freeze the terminology table and target profiles.
- Translate TMS9918A and F18A table/sprite rules into core validation contracts.
- Create independent byte-level fixtures for pattern banks, sprite priority,
  scanline overflow, collision, size, magnification, offscreen placement,
  Graphics I colors, Graphics II bank transitions, and Multicolor nibbles.
- Decide the versioned project-file envelope and safety limits.

Exit criterion: the headless hardware model renders and diagnoses approved
fixtures without a design UI.

### Design Phase 1 — Shared project shell and pixel editor

- Add new/open/save/save-as project workflow with atomic saves and recovery.
- Build the shared indexed-pixel canvas, palette tools, zoom/grid, selection,
  undo/redo, and accessible keyboard commands.
- Add the asset navigator, inspector, diagnostics, and memory summary.

Exit criterion: a small bounded pattern asset can be edited, saved, reopened,
and reproduced byte-for-byte on every platform.

### Design Phase 2 — TMS9918A sprites and composites

- Implement 8x8/16x16 sprite pattern editing and attribute instances.
- Add arbitrary composite layout and anchor/offset manifests.
- Add accurate priority, collision, early-clock, magnification, and scanline
  overflow simulation.
- Export sprite pattern/attribute data and PNG previews.

Exit criterion: the editor correctly identifies and simulates a fifth sprite on
one line and exports a nonrectangular composite reproducibly.

### Design Phase 3 — Character banks, editing, and reservations

- Add one-/two-/three-bank viewers and the mode-aware character editor.
- Add tile/name-table view and linked-use highlighting.
- Implement free/allocated/reserved/locked/conflict states.
- Add deterministic allocation, deduplication, stable re-conversion, and
  previewed remapping.

Exit criterion: a project preserves named reserved ranges while importing and
re-importing a tiled image, and every table reference remains valid.

### Design Phase 4 — Multicolor simulator

- Add the 64x48 logical-cell editor and accurate 256x192 preview.
- Expose pattern, nibble, name-table, and repeated-use views.
- Overlay sprite scenes with hardware scanline behavior.
- Round-trip existing Multicolor conversion tables.

Exit criterion: imported, edited, exported, and re-imported Multicolor tables
produce the same preview and byte data.

### Design Phase 5 — Conversion and export integration

- Create design projects from all applicable existing conversion results.
- Reuse manifest preflight and atomic file writing.
- Add complete raw/TIFILES/V9T9 roles and project manifests.
- Define optional assembler/C/C++/BASIC data dialects separately.

Exit criterion: conversion-to-design-to-export is deterministic and never
silently overwrites source or reserved data.

### Design Phase 6 — F18A enhancement layer

- Add programmable palettes and ECM1/ECM2/ECM3 sprite/tile editing.
- Add per-sprite size/flip, enhanced scanline budget, optional linking, tile
  attributes, and second-layer visualization.
- Add compatibility analysis and assisted downgrade to original TMS9918A.

Exit criterion: every enhanced attribute affects both the preview and emitted
data, and switching target profiles reports every incompatible resource.

### Design Phase 7 — Animation and game-workflow polish

- Add sprite animation frames and onion skinning.
- Analyze worst-case scanline/collision budgets across frames.
- Add metatile libraries, reusable scene templates, and code-symbol naming.
- Connect sprite/pattern sequences to the future Batch Mode manifest where
  useful without making either feature depend on the other.

This phase is optional for the first complete Design Tools release.

## Acceptance criteria

- TMS9918A and F18A previews visibly identify the active profile.
- TMS9918A projects enforce 32 total sprite entries and simulate four per line.
- F18A enhanced sprite limits and ECM colors are simulated only when enabled.
- Composite sprites allow arbitrary signed child placement and stable ordering.
- Hardware and analysis overlap views never imply unsupported color blending.
- Character viewers can show one, two, or three banks and identify every entry
  by bank plus index.
- Editing a reused definition updates every reference, with “make unique”
  available when capacity permits.
- Reserved and locked entries survive allocation, image re-import, compaction,
  save/reopen, and export.
- Graphics I color groups and Graphics II region banks are validated.
- Multicolor editing round-trips the target tables and renders 4x4 logical
  pixels correctly.
- Every export is preflighted and atomic.
- Malformed or oversized project/import data fails within documented limits.
- Project behavior and generated bytes match on Windows, Linux, Intel Mac, and
  Apple Silicon Mac.
- Core hardware rules are tested without requiring the graphical interface.

## Deliberately deferred questions

- Exact project-file extension and JSON/container encoding.
- Which host-machine assembler and BASIC dialects ship first.
- Whether third-party tile/sprite project formats should be imported.
- Animation timing and runtime update-bandwidth analysis.
- Automatic sprite multiplexing/flicker scheduling for original hardware.
- F18A GPU programming, bitmap-layer painting, and full scrolling-map tooling.
- V9938/V9958, Sega VDP, or other related but distinct video chips.
- Collaborative/cloud project storage.

These may build on the shared hardware model later but must not broaden the
initial TMS9918A/F18A authoring scope unnoticed.

## Technical references

- [Texas Instruments Video Display Processor Programmer's Guide](https://dnotq.io/f18a/TI-VDP-Programmers_Guide.pdf)
  — original display modes, tables, and sprite behavior.
- [Texas Instruments TMS9918A/TMS9928A/TMS9929A data manual](https://www.bitsavers.org/components/ti/TMS9900/TMS9918A_TMS9928A_TMS9929A_Video_Display_Processors_Data_Manual_Nov82.pdf)
  — chip-level memory and display contracts.
- [F18A introduction and feature reference](https://dnotq.io/f18a/intro.html)
  — enhanced palettes, color modes, sprites, attributes, and tile layers.
- [F18A pin-out and compatibility jumpers](https://dnotq.io/f18a/pinout.html)
  — original versus enhanced sprite-line compatibility defaults.
