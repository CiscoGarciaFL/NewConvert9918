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
full-image conversion preview. The initial Character and Sprite modes provide
intentional editor/import placeholders while their hardware editors are built.
Source remains visible and unchanged in every mode. The contextual Side Panel
switches with the destination, using the headers **Image Settings**,
**Character Options**, and **Sprite Options**. File → Export is disabled while
an editor mode is active so it cannot export a hidden Screen Image result
accidentally.

Character Editor and Sprite Editor use the same destination-pane shell as
Screen Image: matching title styling, reserved upper toolbar space, a bordered
viewport, and the same lower toolbar layout with zoom controls. Their zoom
controls remain disabled while the placeholder has no editor content.

Every Side Panel exposes a required TMS9918A baseline, an optional F18A output
that inherits from it, and a non-mutating 9918A/F18A/Compare preview selector.
Character Editor starts with three selectable 256-pattern sets. Sprite Editor
uses repeatable 32-sprite sets and can switch between its slot grid and a
placement box over the current Screen Image reference. File → Load Recipe and
Save Recipe persist this foundation and all current conversion settings in a
versioned `*.nc9918.json` file shared with the CLI.

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
