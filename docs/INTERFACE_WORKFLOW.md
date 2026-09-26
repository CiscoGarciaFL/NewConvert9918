# Interface workflow

Phase 6 connects the Qt Quick interface to the portable conversion and export
libraries. The primary path is deliberately linear:

1. Open, drop, paste, or pass an image on the command line.
2. Choose fit/crop positioning and a scaling filter.
3. Choose a target mode or apply a named starting preset.
4. Adjust common settings, then expand Advanced only when needed.
5. Inspect the live Screen Image preview, palette, and generated-file summary.
6. Choose File → Export and a destination folder.

File → Reload (`Ctrl+R`) reopens the current file-backed source while
retaining the active conversion settings. It is disabled until a source file
has been loaded and remains disabled for clipboard-only images.

With Auto enabled, changing a conversion setting starts a 160 ms debounce.
With Auto disabled, changes are marked pending and conversion waits for the
compact Update button in the Screen Image Side Panel header. Work then runs through Qt's
background thread pool, leaving the interface responsive. Each request has a
cancellation token and generation number; a completed result is published only
if it is still the newest request. The last valid preview remains visible while
a replacement is calculated.

The persistent **Live** switch beside Update optionally replaces that last
preview with real completed rows as conversion proceeds. Updates are throttled
to eight-row intervals, and two-pass Half Multicolor reports both passes.
Turning Live off removes all intermediate image creation and PNG encoding from
the normal conversion path. Generation checks also discard queued partial
frames from canceled or superseded work.

## Preview and framing

The Source and Screen Image panes each provide mouse-wheel zoom, panning, and a
compact bottom ribbon. A magnifier and live zoom percentage open a menu with
Fit to view and Actual size (1:1), while half-height zoom-in and zoom-out
buttons are stacked beside it. This zoom family remains right-aligned. Source
framing controls occupy the left side of that same ribbon and wrap at narrow
pane widths. View → Tabbed
places the panes on Source and a mode-dependent destination tab and suppresses the redundant
title inside each pane. View → Horizontal uses an adjustable side-by-side
split, and View → Vertical uses an adjustable top-and-bottom split. Horizontal
is the default. Both split arrangements start with equal pane sizes and retain
an adjustable separator.

Mode selects one of three mutually exclusive destination workspaces: Screen
Image, Character Editor, or Sprite Editor. Screen Image contains the existing
full-image conversion preview. Character and Sprite provide hardware-aware
pixel editors and placement presentations; Source import remains intentionally
disabled until its mapping preview is implemented.
Source remains visible and unchanged in every mode. The contextual Side Panel
switches with the destination, using the headers **Image Settings**,
**Character Options**, and **Sprite Options**. File → Export is disabled while
an editor mode is active so it cannot export a hidden Screen Image result
accidentally.

Character Editor and Sprite Editor use the same destination-pane shell as
Screen Image: matching title styling, reserved upper toolbar space, a bordered
viewport, and the same lower toolbar layout with zoom controls. Character mode
keeps the pane zoom fixed and non-interactive at 100%; each pattern editor owns
its separate 1x/2x/3x/4x character preview. The Character tray otherwise uses
a fixed hardware-pixel grid. Sprite pixel-edit mode likewise remains at 100%; its
zoom control becomes active only for the full placement screen and scales that
screen and the containing tray together.

Every Side Panel exposes a required TMS9918A baseline, an optional F18A output
that inherits from it, and a non-mutating 9918A/F18A/Compare preview selector.
Character Editor starts with three selectable 256-pattern sets. Above the slot
grid, its adaptive Graphics II editor tray hosts one or more 8x8 editors, shows
row pattern/color bytes, and uses the pane's upper toolbar for pencil, eraser,
and indexed TMS9918A or F18A foreground/background colors. One editor is always
present. Its inverted pattern label marks the active destination, new editors
start empty, and selecting a pattern slot loads it into that active editor.
Bottom-right plus/minus controls add and remove editors; each label also opens
a local menu for reordering or removal. The editors wrap as the pane width
changes and the tray grows or scrolls as needed. Pattern data and tray layout
are retained in recipes. Each editor includes a bottom-right-anchored preview
under its color-byte column; stacked plus/minus controls cycle its exact 1x
through 4x pixel sizes. Vertical byte-column captions allow the grid to move to
the top margin while the pattern label remains bottom-aligned. A compact
right-aligned Set/Pattern row immediately
above the slot grid replaces the earlier set tabs and duplicate selection text;
Character mode therefore leaves the lower toolbar available for future
editor-wide commands. A single upper-toolbar mode button replaces the separate
Pattern Editor and Tiling tools. It uses overlapping Pattern Editor and Tiling
Screen icons, brings the current mode to the front, and names the destination
mode in its tooltip. Tiling replaces the editor tray with a 1:1, 256x192 screen
grid while retaining the same slots, active pattern, and plus/minus controls.
Tiles drag and snap in 8x8 character cells from the
upper-left Home origin. Duplicate instances of one pattern highlight together
but keep independent coordinates: press `+` for an empty tile, then select the
same pattern again to assign it. Pattern Editor mode collapses those loaded
duplicates into one editor per `(set, pattern)` while Tiling retains every
positioned instance; empty unassigned slots remain available for assignment.
Tile labels appear only on hover. Recipe
files retain the Tiling presentation and every tile position. In this
presentation only, the destination zoom family scales the complete screen grid;
the tray requests the corresponding scaled screen height, remains responsive to
the current window width, and falls back to scrolling when space is exhausted.
Clicking the combined mode button returns to Pattern Edit mode and fixes the
pane zoom at 100% again.
The Character upper toolbar adds clockwise Rotate, horizontal Mirror, vertical
Flip, and starburst Blank tools. The lower toolbar adds Character undo/redo and
a Pan toggle with left, up, center, down, and right controls. Pan uses a
temporary 24x24 virtual grid around the visible center 8x8, preserving clipped
pixels until Pan is turned off. Finishing Pan commits the final viewport,
returns the virtual origin to center, and contributes one history entry for the
entire positioning session. Drawing strokes are likewise grouped into single
undo entries.
Character Copy and Paste also live in the upper toolbar and exchange an
indented, versioned `newconvert9918.character-pattern` JSON object through the
system clipboard. The JSON exposes eight hexadecimal bitmap bytes and eight
hexadecimal row-color bytes for inspection or scripting. Paste validates the
payload and replaces the active pattern as one undoable edit.
Sprite Editor uses repeatable sets containing two simultaneous definition banks:
32 8x8 patterns and 32 16x16 patterns. One active pixel editor sits above the
stacked thumbnail banks and uses the same pencil, eraser, color, rotate, mirror,
flip, Blank, JSON Copy/Paste, grouped undo/redo, and virtual-grid Pan workflow as
Character Editor. In shared-baseline scope, selecting a bank also selects the
single global TMS9918A sprite size and pixels are transparent/opaque with one
instance color. F18A scope retains the baseline until a sprite is changed, then
stores a non-destructive override with independent 8x8/16x16 size and 1-, 2-, or
3-bpp indexed pixels for that sprite.

The combined upper-toolbar mode control switches between pixel editing and a
placement screen over the current Screen Image reference. All 32 instances may
overlap and drag at one-hardware-pixel resolution; their displayed size follows
the TMS9918A global setting or the F18A per-sprite setting. Placement mode enables
destination zoom and grows its tray with the scaled screen. File → Load Recipe
and Save Recipe persist both banks, baseline and F18A pixel data, override state,
size/color-depth attributes, coordinates, tool selections, and current
conversion settings in the versioned `*.nc9918.json` format shared with the CLI.

Pane and settings-group outlines follow the active Qt palette: dark outlines
in light mode and light outlines in dark mode. This keeps the section framing
visible when the operating system changes its color theme.

The Source pane displays the exact framed 256×192 input without running the
converter. Its compact toolbar moves the image one pixel in four directions,
centers it, chooses a background fill from the working TMS9918A palette, or
uses an eyedropper to map a source pixel to the nearest palette entry. This
makes framing and letterbox-fill adjustments immediate even when Auto is off.
Start, center, and end crop modes plus the scaling filter remain in the
Screen Image Side Panel.

The Palette section shows the active target colors. Scanline Palette Bitmap
F18A additionally offers a 16-by-192 map of the palette selected for every
output row.

The project logo supplies the application/window icon. Empty Source and
Screen Image viewports show the same mark at one-half of the shorter viewport
dimension with subdued opacity. All logo assets use a transparent outer
background. The canonical SVG and generated 16–1024 pixel PNG, Windows ICO,
and macOS ICNS assets live under `app/assets`; rerun
`tools/generate_app_icons.py` after changing the SVG.

## Settings and presets

Balanced restores the compatibility defaults. Crisp pixel art disables
dithering and resampling, Smooth photograph enables histogram stretching and
the Blackman filter, and Ordered retro selects ordered dithering. Undo groups
rapid slider changes into one action; Reset restores all conversion and
framing defaults. The most recent settings and export choice are stored with
`QSettings` in the operating system's normal per-user settings location.

The Screen Image Side Panel uses compact light-blue disclosure headers. Art Style
starts expanded so a new user can immediately choose and apply Balanced,
Crisp pixel art, Smooth photograph, or Ordered retro. Common settings,
Framing and scale, Advanced settings, Working palette, and Export start
collapsed and reveal their controls directly beneath the header without a
redundant second title. Working palette appears only for modes that use the
editable TMS9918A palette.

Common settings contains the target and dither choices plus the frequently
used matching and framing switches. Maximum color shift, gamma, luma
emphasis, flicker limits, ordered brightness, and Average/Accumulate
distributed-error handling are available under Advanced.
Ordered dithering also exposes its 2×2 or 4×4 threshold map there; these map
to the original Order 1/2 and Order 3/4 choices respectively.
The six compact directional weight controls reproduce the original
down-left, down, down-right, right, far-right, and two-rows-down diffusion
cells. Selecting Floyd–Steinberg, Atkinson, Pattern, Diagonal, or Ordered with
error loads that method's weights; editing any weight selects Custom. Each
cell is validated from 0 through 16, the total is shown in sixteenths, and the
custom kernel participates in persistence, undo, reset, and live conversion.
Ordered darkening uses the original 0–16 range, where larger values darken the
ordered threshold result. Perceptual matching exposes editable R/G/B weights
with a one-click 30/52/18 restore, and maximum color shift exposes its complete
0–100% range.

The Working palette section edits all fifteen TMS9918A colors with the native
color picker and can restore the audited defaults. F18A modes expose Median
Cut or Popularity selection. Scanline F18A additionally offers 0–14 shared
colors and Region 1/2/3 inclusion; shared entries retain stable palette slots
across the image. PowerPaint framing prepares a 240×160 upper-left active area
with opaque black padding to the normal 256×192 target.

The final parity decisions and compatibility-gated exclusions are recorded in
`PHASE6A_PARITY_REVIEW.md`.

## Export feedback

Before a folder is selected, the Export section lists every filename, its byte
count, and the manifest total. Formats that do not apply to the active target
say why they are unavailable. The interface exposes the formats that can be
created without external machine-code templates; the ROM and Extended BASIC
template APIs remain available at the library boundary described in
`docs/EXPORT_FORMATS.md`.

No export file is changed until the complete manifest passes preflight. If any
name already exists, the confirmation dialog lists all conflicts and requires
an explicit Replace choice.

## Keyboard and responsive layout

| Shortcut | Action |
| --- | --- |
| `Ctrl+O` | Open an image |
| `Ctrl+V` | Paste an image |
| `Ctrl+E` | Export the current result |
| `Ctrl+Q` / `Cmd+Q` | Exit the application |
| `Ctrl+Z` | Undo the last settings group |
| `Ctrl+0` | Reset conversion settings |
| `F1` | Open About and attribution |

Interactive controls participate in tab navigation and provide accessible
names where their visible label is not sufficient. The Fusion control style
provides consistent contrast across the supported desktops. File contains
Open, Paste, Export, and a separated Exit command, while Help contains About.
View selects the preview arrangement, hides or shows the Side Panel, and
places that panel either
adjacent to the workspace or in a non-modal overlay above it. The overlay has
a hide control in its top-right corner and automatically hides after the mouse
leaves it. While hidden, a frameless right-edge rail provides an expand button
whose themed background remains visible; its tooltip still appears only on
hover.

## Verification

`interface_workflow_validation` exercises image load, debounced and manual
conversion, non-converting source reframing, palette-constrained background
selection, stale-result rejection, persistence, undo, scanline palette
visualization, manifest export, overwrite preflight, QML loading, all three
preview layouts, hidden/adjacent/overlay panel states, overlay hide and expand
controls and their visible/frameless styling, disclosure-section defaults,
bottom-positioned preview controls, narrow/wide sizing, clean QML teardown,
the Screen Image/Character Editor/Sprite Editor mode transitions and contextual
Side Panels, and a rendered frame at 125% scale. The Windows development pass also renders
through the native platform plug-in to verify real system fonts and DPI
behavior. Conversion algorithms and export bytes remain covered by their
independent core and golden tests.
