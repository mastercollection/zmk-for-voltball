# Prospector landscape mascot

280×240 layout: layer at top left; Scroll / Caps / NumLock at top right;
transparent animated mascot in the center; upstream Prospector modifier symbols
on the right; left/right batteries and WPM at the bottom. No USB indicator.

- Caps and NumLock use the active host's HID LED report. Caps-word lights the
  Caps box in amber (#FFB802) when Caps Lock is off; the Shift symbol on the
  right shows only a held Shift. The Scroll box lights
  while `&ptr` scroll mode is on (toggle or hold).
- A speed key press (`&ptr PTR_SPD_*`) shows the current ratio, e.g. `x3/2`, in
  the WPM slot for one second in the accent color, then WPM returns.
- Typing starts the mascot immediately and it sits again 850 ms after the last
  non-modifier key-down. Below 40 WPM it strolls with its shopping bags at
  100 ms per frame; from 40 WPM it rides the scooter at 120–50 ms per frame
  based on ZMK WPM, and walks again only once WPM drops below 30.
- Battery labels show OFF when disconnected and N/A until an actual report.
  Pair left first, then right, as with the existing Prospector configuration.
- Battery bars are green (#7BD88F) at 50–100%, amber (#FFB802) at 20–49%,
  and red (#FF6B6B) below 20%. Numeric labels stay neutral (#DDD7DF);
  OFF/N/A labels use gray (#98909F).
- Default landscape orientation is retained. Flip the existing
  CONFIG_PROSPECTOR_ROTATE_DISPLAY_180 option for the opposite mounting direction.

Sources: `source-sheet.png` is the approved transparent four-pose image made from
the user's reference; only its sitting pose is used, as the idle frame.
`walk-sheet.png` holds the six walking frames of the user's doro.mp4 clip and
`scooter-sheet.png` one five-frame kick cycle (frames 8–12) of the user's
scooter GIF, both with the white video background removed by a flood fill from
the border so the white body survives. `pack_mascot.py` (Pillow) first tidies
those video frames: the two outermost opaque pixels, blends of the outline with
the old white background, become black, and dark pixels snap to pure black so
the outline is crisp without compression noise. It then scales every frame to
132 px tall and packs it as an LVGL I8 image: a 256-entry ARGB8888
palette plus one byte per pixel, index 0 transparent. Partly transparent edge
pixels are stored pre-blended over black, which is what lies behind the mascot.
I8 keeps the eye and ribbon colors that a 16-color palette loses and costs about
a third of RGB565A8, which is what makes twelve frames fit the xiao_ble flash.
The checked-in `mascot.i8` is deterministic build input; CI needs no image
library. `tests/patch_modules.py` installs this UI only into the exact pinned
Prospector revision and converts the binary into constant C image arrays. LVGL
decodes I8 C arrays line by line (`LV_BIN_DECODER_RAM_LOAD=n`), so no frame is
expanded in RAM.

Only the normal Prospector UF2 is needed for this display update. Do not use a
settings_reset image for a routine update. Keep the previous firmware for rollback.
The source tests and CI build cannot verify physical LCD orientation, animation
smoothness, host lock reports, or radio/battery behavior; check those on hardware.
