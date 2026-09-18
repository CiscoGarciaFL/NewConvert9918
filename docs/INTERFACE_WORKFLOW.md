# Interface workflow

Phase 6 connects the Qt Quick interface to the portable conversion and export
libraries. The primary path is deliberately linear:

1. Open, drop, paste, or pass an image on the command line.
2. Choose fit/crop positioning and a scaling filter.
3. Choose a target mode or apply a named starting preset.
4. Adjust common settings, then expand Advanced only when needed.
5. Inspect the live converted preview, palette, and generated-file summary.
6. Choose File → Export and a destination folder.

Changing a conversion setting starts a 160 ms debounce. Work then runs through
Qt's background thread pool, leaving the interface responsive. Each request has
a cancellation token and generation number; a completed result is published
only if it is still the newest request. The last valid preview remains visible
while a replacement is calculated.

## Preview and framing

The source and converted panes each provide mouse-wheel zoom, panning, and a
bottom-right control group for zoom out, fit, 1:1, and zoom in. View → Tabbed
places them on Source and Converted tabs and suppresses the redundant title
inside each pane. View → Horizontal uses an adjustable side-by-side split, and
View → Vertical uses an adjustable top-and-bottom split. Horizontal is the
default.

Fit/crop changes update a 4:3 framing overlay on the source and schedule a
fresh preview. Start, center, and end crop modes expose horizontal and vertical
positioning controls.

The Palette section shows the active target colors. Scanline Palette Bitmap
F18A additionally offers a 16-by-192 map of the palette selected for every
output row.

## Settings and presets

Balanced restores the compatibility defaults. Crisp pixel art disables
dithering and resampling, Smooth photograph enables histogram stretching and
the Blackman filter, and Ordered retro selects ordered dithering. Undo groups
rapid slider changes into one action; Reset restores all conversion and
framing defaults. The most recent settings and export choice are stored with
`QSettings` in the operating system's normal per-user settings location.

Common target, dither, framing, and color options stay visible. Maximum color
shift, gamma, luma emphasis, flicker limit, and ordered-dither brightness are
kept in the collapsible Advanced group.

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
View selects the preview arrangement, hides or shows the Conversion panel, and
places that panel either
adjacent to the workspace or in a non-modal overlay above it. The overlay has
a hide control in its top-right corner and automatically hides after the mouse
leaves it. While hidden, a narrow right-edge rail provides its expand control.

## Verification

`interface_workflow_validation` exercises image load, debounced conversion,
stale-result rejection, persistence, undo, scanline palette visualization,
manifest export, overwrite preflight, QML loading, all three preview layouts,
hidden/adjacent/overlay panel states, overlay hide and expand controls,
bottom-positioned preview controls, narrow/wide sizing, clean QML teardown,
and a rendered frame at 125% scale. The Windows development pass also renders
through the native platform plug-in to verify real system fonts and DPI
behavior. Conversion algorithms and export bytes remain covered by their
independent core and golden tests.
