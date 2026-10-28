# Prospector landscape mascot

280×240 layout: layer at top left; Caps / NumLock at top right;
transparent animated mascot in the center; upstream Prospector modifier symbols
on the right; left/right batteries and WPM at the bottom. No USB indicator.

- Caps and NumLock use the active host's HID LED report.
- Typing starts the three-pose run cycle immediately. It stops 850 ms after the
  last non-modifier key-down. Frame interval is 120–50 ms based on ZMK WPM.
- Battery labels show OFF when disconnected and N/A until an actual report.
  Pair left first, then right, as with the existing Prospector configuration.
- Default landscape orientation is retained. Flip the existing
  CONFIG_PROSPECTOR_ROTATE_DISPLAY_180 option for the opposite mounting direction.

`source-sheet.png` is the approved transparent four-pose image made from the
user's reference using the built-in image generator in this design session.
`pack_mascot.py` (Pillow) packs it as four 160×132 RGB565A8 frames. This conversion
retains per-pixel transparency; no opaque rectangle is added behind the character.
The checked-in binary is deterministic build input; CI needs no image library.
`tests/patch_modules.py` installs this UI only into the exact pinned Prospector
revision and converts the binary into constant C image arrays.

Only the normal Prospector UF2 is needed for this display update. Do not use a
settings_reset image for a routine update. Keep the previous firmware for rollback.
The source tests and CI build cannot verify physical LCD orientation, animation
smoothness, host lock reports, or radio/battery behavior; check those on hardware.
