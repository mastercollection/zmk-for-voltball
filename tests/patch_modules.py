"""Apply reviewed compatibility fixes to exact pinned external module sources in CI."""
from pathlib import Path
import re
import struct
import subprocess
import sys
import shutil

workspace = Path(sys.argv[1])
from patch_auto_mouse import apply as apply_auto_mouse
apply_auto_mouse(workspace / "zmk")

module = workspace / "prospector-zmk-module"
expected = "ed98221f3b52b7066dbb10ba3af8a29150b93a5a"
actual = subprocess.check_output(["git", "-C", str(module), "rev-parse", "HEAD"], text=True).strip()
if actual != expected:
    raise SystemExit(f"Prospector revision changed: {actual}")
# The CI module cache can hold a previously patched tree; start from the pinned commit.
subprocess.run(["git", "-C", str(module), "checkout", "--", "."], check=True)
path = module / "boards/shields/prospector_adapter/src/layouts/classic/battery_bar.c"
source = path.read_text()
old = "return (struct battery_update_state){.source = 0, .level = 0};"
if source.count(old) != 2:
    raise SystemExit("Unexpected battery widget source; review patch before building")
# Invalid source is ignored by set_battery_bar_value. Preserve initial N/A until an actual report.
source = source.replace(old, "return (struct battery_update_state){.source = UINT8_MAX, .level = 0};")
path.write_text(source)
print(f"Patched Prospector {actual}: retain N/A before first peripheral report")

# Install the reviewed landscape UI without following mutable upstream branches.
assets = Path(__file__).resolve().parents[1] / "display"
layout = module / "boards/shields/prospector_adapter/src/layouts/classic"
for name in ("status_screen.c", "mascot_logic.h"):
    shutil.copyfile(assets / name, layout / name)
# Packed I8 frames: u16 width, u16 height, 256 ARGB8888 palette entries, then
# one index per pixel. Index 0 is transparent. mascot_logic.h fixes the order.
packed = (assets / "mascot.i8").read_bytes()
logic = (assets / "mascot_logic.h").read_text()
expected_frames = 1 + sum(int(re.search(rf"#define MASCOT_{name}_FRAMES (\d+)", logic).group(1))
                          for name in ("WALK", "RUN"))
frames, offset = [], 0
while offset < len(packed):
    width, height = struct.unpack_from("<HH", packed, offset)
    size = 1024 + width * height
    frames.append((width, height, packed[offset + 4:offset + 4 + size]))
    offset += 4 + size
if offset != len(packed) or len(frames) != expected_frames or any(
        len(data) != 1024 + w * h or w % 4 or h != 132 for w, h, data in frames):
    raise SystemExit("Unexpected mascot asset layout")
lines = ['#include <lvgl.h>']
for frame, (width, height, data) in enumerate(frames):
    lines.append(f'static const uint8_t mascot_data_{frame}[] __attribute__((aligned(4))) = {{')
    for start in range(0, len(data), 24):
        lines.append(','.join(f'0x{b:02x}' for b in data[start:start + 24]) + ',')
    lines.append('};')
lines.append(f'const lv_image_dsc_t mascot_frames[{len(frames)}] = {{')
for frame, (width, height, data) in enumerate(frames):
    lines.append('{.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_I8, '
                 f'.w = {width}, .h = {height}, .stride = {width}}}, '
                 f'.data_size = {len(data)}, .data = mascot_data_{frame}' + '},')
lines.append('};')
(layout / "mascot_assets.c").write_text('\n'.join(lines) + '\n')
# Keep the actual default modifier widget, including its ordering and OS symbols.
# The new screen does not need the old roller/battery/output widgets or their fonts.
# Caps-word is shown on the Caps box by status_screen.c; the Shift symbol then
# reflects only a held Shift instead of also lighting for caps-word.
mods_path = layout / "modifier_indicator.c"
mods_source = mods_path.read_text()
old = "        .caps_word = caps_word_active,"
if mods_source.count(old) != 1:
    raise SystemExit("Unexpected modifier widget source; review caps-word patch before building")
mods_path.write_text(mods_source.replace(old, "        .caps_word = false,"))
cmake = module / "boards/shields/prospector_adapter/CMakeLists.txt"
source = cmake.read_text()
old = 'file(GLOB layout_sources src/layouts/classic/*.c)'
assert source.count(old) == 1
source = source.replace(old, 'set(layout_sources src/layouts/classic/modifier_indicator.c '
                        'src/layouts/classic/mascot_assets.c)')
cmake.write_text(source)
print(f"Installed landscape mascot screen, {len(frames)} I8 frames, and default modifiers")
