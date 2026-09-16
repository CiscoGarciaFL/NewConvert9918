# Golden-test corpus

This directory contains the approved source-image corpus and captured outputs
for behavioral comparison with the original Convert9918 executable. Expected
previews and binary exports live under per-case result directories.

`corpus.json` is the machine-readable inventory. It records each file's
purpose, dimensions, color representation, provenance, and SHA-256 digest.
The `core_validation` test suite verifies those digests, confirms that every
valid PNG decodes at its recorded dimensions, and confirms that malformed
cases fail.

`reference/original-1_9_1/capture.json` records the first approved original
capture. Each valid source was converted in default Bitmap 9918A mode through
the executable's command-line path. Each case contains the original 256×192
BMP preview and its TIFILES pattern (`TIAP`) and color (`TIAC`) tables. The
directory name deliberately avoids dots because the original save code
mistakes a dot anywhere in the full path for a filename extension.

`reference/original-1_9_1/mode-captures.json` extends that baseline across the
other eight conversion modes. It records each applicable TIFILES table and the
command-line BMP preview. Scanline Palette Bitmap F18A has no BMP because the
original deliberately suppresses its normal single-palette preview path for
per-scanline palettes.

## Coverage

| Fixture | Primary coverage |
| --- | --- |
| `photo-landscape.png` | Photographic detail, broad color range, landscape |
| `pixel-art-portrait.png` | Pixel art, hard edges, portrait |
| `flat-cartoon-square.png` | Illustration, large flat regions, square |
| `greyscale-gradient.png` | Native 8-bit greyscale and non-power-of-two size |
| `black-white-pattern.png` | Native one-bit black and white |
| `tiny-rgba.png` | Very small source and alpha extremes |
| `transparency-rgba.png` | Smooth alpha gradient |
| `large-pattern.png` | 4096×3072 allocation and size boundary |
| `malformed/*.png` | Empty, invalid-signature, and truncated input rejection |

## Provenance and redistribution

The photo, pixel-art, and cartoon fixtures were generated specifically for
this project with OpenAI image generation on 2026-09-15. The prompt summaries
are preserved in `corpus.json`. They do not intentionally depict real people,
protected characters, brands, logos, or readable text.

OpenAI's Terms of Use effective 2026-01-01 state that, as between the user and
OpenAI and to the extent permitted by applicable law, the user owns generated
output. The project owner is publishing these fixtures under this repository's
`LICENSE`. The terms were checked at:
<https://openai.com/policies/row-terms-of-use/#content>.

The other fixtures are deterministic outputs of
`tools/generate_test_corpus.py` and are likewise covered by the repository
license. Run that script from the repository root whenever the procedural
fixtures or manifest need to be regenerated. The three image-generated files
must already be present because their exact bytes are intentionally pinned.

On Windows, `tools/capture_original_baseline.ps1 -Executable <path>` recreates
the default Bitmap 9918A capture from the verified original 1.9.1 executable.
It refuses a different executable hash and refuses to overwrite a nonempty
capture directory.

Run `tools/capture_original_modes.ps1 -Executable <path>` to recreate the
Greyscale, B&W, multicolor, color-only, and F18A mode captures. It applies the
same executable-hash and no-overwrite safeguards.

No original Convert9918 images, ImgSource material, or third-party stock media
are included in this corpus.
