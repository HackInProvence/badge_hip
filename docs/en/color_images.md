# Images revealed by red or blue light

A color image, on a screen or printed, that hides two patterns: one appears under a red light, the other under a
blue light. The LEDs of the badges are the lamp: a CTF riddle, a welcome message, a treasure hunt in a dark room...

*Version française : [images_couleur.md](../fr/images_couleur.md).*

![Screen example: no filter, red filter, blue filter](../color_reveal/secsea_cicada_screen_preview.png)

## The principle

Under a light of a single color, only the part of that color counts, and the image reads in shades of grey:

| Color printed or shown | under red light | under blue light |
|---|---|---|
| white | light | light |
| red, yellow, orange | light | **dark** |
| magenta, pink | light | light (a little less on paper) |
| blue | **dark** | light |
| cyan | **dark** | light |
| green, black | **dark** | **dark** |

This is the old trick of the "decoder glasses": a message written in blue, scribbled over in red, reads through a
red filter, which makes the red as light as the paper. The tool does it on both colors at once, with colored cells
(Ishihara-test-like dots, blobs or stripes):

- the **red part** of a cell says whether it is in pattern A (dark under red light);
- its **blue part** says whether it is in pattern B (dark under blue light), independently;
- its **green part** (the magenta ink on paper), which neither light sees, is the camouflage: cells more or less
  green / pink at random, soft blobs, and an optional **decoy** (`--decoy`), a text the eye reads at once and that
  vanishes under both lights;
- a **haze** (`--haze`): wide blurry blobs of red and blue over the whole image. Under the light, the eye reads the
  sharp edges of the pattern through it; with the naked eye, it sees changes of hue everywhere, not only on the
  pattern.

On paper, the tool computes the inks directly: cyan for the red pattern, yellow for the blue pattern, magenta for
the camouflage, no black (black would be dark under both lights). It corrects the unwanted absorption of the
magenta in the blue (real magenta ink takes part of the blue light) with the amount of yellow.

## The commands

Python 3 with Pillow and numpy (`pip install pillow numpy`).

```
python tools/color_reveal.py --text-red SECSEA --image-blue @cicada -o reveal.png --preview
python tools/color_reveal.py --text-red SECSEA --text-blue "HIP|2026" --decoy "HELLO" --style blobs -o hip.png --preview
python tools/color_reveal.py --text-red SECSEA --text-blue "HIP|2026" --print --size 150x100mm --dpi 300 -o hip_print.png --preview
python tools/color_reveal.py --image-red logo.png --text-blue "FLAG{...}" --print --cmyk flag_cmyk.tif -o flag.png
```

| Option | Role |
|---|---|
| `--text-red`, `--text-blue` | the text that appears under the red / blue light (`\|`: new line) |
| `--image-red`, `--image-blue` | a black and white picture (black = the pattern, `--invert` for the opposite; transparency counts as white), or `@cicada`, a drawn cicada |
| `--decoy` | a decoy text, seen with the naked eye, gone under both lights |
| `--style` | `dots` (default: the best hidden), `blobs` (the most readable under the light), `stripes` |
| `--cell` | the size of the cells in pixels (default: 1/60 of the height) |
| `--strength` | 0.1 to 1: the contrast of the patterns (default 0.7); lower: better hidden but harder to read |
| `--haze`, `--noise` | the haze of red and blue (0.25) and the share of cells flipped at random (0.06) |
| `--size` | `WxH` in pixels (default 1200x600) or `WxHmm` with `--dpi` (150x100mm in print mode) |
| `--print`, `--dpi`, `--cmyk` | ink colors for printing, the resolution (300 dpi), and a CMYK TIFF (K = 0) for a print shop |
| `--seed` | the random draw (same seed: same image) |
| `--preview` | also writes `<output>_preview.png`: the image, its view under red light and under blue light, side by side (a simulation, to check without a lamp) |

The examples in [docs/color_reveal](../color_reveal/): `secsea_cicada_screen` (screen: SECSEA in red, a cicada in
blue), `secsea_hip2026_decoy_screen` (screen, blobs, decoy "HELLO"), `secsea_hip2026_print` (print, 150 x 100 mm at
300 dpi), `secsea_cicada_decoy_print` (print with the decoy "CTF"), and their `_preview`.

## Printing

- **Printer**: inkjet gives the purest colors (transparent inks); a color laser works too. Print at 100 % (not "fit
  to page" if the size matters), without "vivid colors" or automatic photo enhancement.
- **Paper**: a **matte coated** (or satin) paper is the best compromise: saturated colors, and no reflection of the
  LED. Glossy (photo) paper gives the strongest colors but sends the lamp back as a bright spot: light it at an
  angle. Office paper works, with less contrast (the ink soaks in and spreads: keep cells of at least 1.5 mm, which
  the defaults give at 150 x 100 mm).
- **At a print shop**: give the CMYK TIFF (`--cmyk`), with no profile conversion and no replacement of grey by black
  ("GCR / UCR" off): the tool uses no black ink on purpose.
- Always **make a test print** and look at it under the LEDs before printing many: every printer + paper pair is
  different. If a pattern is too visible with the naked eye, lower `--strength` (0.5); if it is too faint under the
  light, raise it (0.9) or make the cells bigger (`--cell`).

## Lighting the paper

- **A dark room**: any white light around (window, ceiling light, screen) washes the effect out. The darker the
  room, the more the pattern stands out.
- **The LEDs of the badge**: Admin > **Cicada LEDs** (LEDs des cigales), "Color" red (or Red 255, Green 0, Blue 0),
  "Mode" Fixed: the LEDs of this badge show the color while it is set; "> Send to the cicadas" turns every cicada
  around red (handy for a whole room or a workshop). Then blue (0, 0, 255). Hold the badge 10 - 30 cm from the
  sheet; several badges together light better. Outside the admin mode, a red and a blue lamp do the same.
- **A torch** with a filter: a primary red and a deep blue lighting gel (stage filters) work well; colored cellophane
  lets other colors through (the blue one especially lets green through): the effect is weaker. A colored LED (red,
  blue) is always purer than a filtered white lamp.
- **Without a lamp**: looking at the sheet, in white light, through a red then a blue filter (like decoder glasses)
  gives the same effect, with less contrast.

## On a screen

A screen gives its own light: lighting it in red changes nothing. **Look at the screen through a filter**: red for
pattern A, blue for pattern B. The red, green and blue subpixels of the screen are well apart, so this is the
cleanest case, if the filter is clean too:

- the **red** lens of red / cyan 3D glasses works very well;
- the **cyan** lens does **not** work for the blue pattern: it lets the green through, so the camouflage. You need a
  blue filter that cuts green (a deep blue gel, or several layers of blue cellophane);
- turn off "night light / True Tone / eye comfort", which removes blue;
- a picture of the screen or the file, opened in an image editor that shows a single channel (red or blue), also
  reveals the patterns: a classic of the steganography challenges of CTFs (StegSolve, GIMP > Colors > Components >
  Decompose).

## Limits

- **Never perfectly invisible to the naked eye**: the pattern is a real difference of color. The simulations show
  that on a screen, with the defaults and dots, a text can barely be guessed (a vague bluish or orange area if you
  look for it); with a decoy, the eye reads the decoy and stops looking. Large solid shapes (a filled cicada, a
  ring) show more than a text in thin strokes.
- **Paper hides less well than a screen**: the cyan ink of the red pattern stays visible as small blue-green dots,
  and the magenta camouflage is limited so as not to darken the blue view. Use `--decoy` and a `--strength` of 0.5
  to 0.6 on paper.
- **The simulations are rough**: they assume a pure red and a pure blue light and average inks. The real inks, the
  paper, the screen and the filters change the result: always check for real.
- The red and the blue patterns may overlap without spoiling each other; where both are, the cell is green or dark.
- A color-blind person will not see the image with the naked eye like the others (sometimes the pattern better,
  sometimes less).

## Sources

- [Museums Victoria, *Hidden messages in colour*](https://museumsvictoria.com.au/scienceworks/at-home/play/hidden-messages-in-colour/):
  why red vanishes behind a red filter and blue turns dark.
- [Rainbow Symphony, *Secret Message Glasses*](https://www.rainbowsymphony.com/blogs/blog/are-decoder-glasses-really-a-thing):
  a cyan message hidden by reds (red filter), a yellow message (blue filter).
- [Curiokids, *Decode messages with a red filter*](https://curiokids.net/en/decode-messages-with-a-red-filter/) and
  [Fleet Science Center, *Hidden Messages*](https://www.fleetscience.org/sites/default/files/Hidden%20Messages%20(Printable%20Instructions).pdf).
- Xerox patents on "spectral multiplexing" (several images in one print, each revealed by a lighting):
  [US 7379588](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/7379588),
  [US 7269297](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/7269297) (which also describes the
  unwanted absorptions of the inks, magenta most of all).
- [US 10453162](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/10453162): under a red light, the
  areas without red-absorbing ink look white, green and blue inks black.
