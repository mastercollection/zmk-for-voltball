"""Apply reviewed compatibility fixes to exact pinned external module sources in CI."""
from pathlib import Path
import subprocess
import sys
import shutil

workspace = Path(sys.argv[1])
module = workspace / "prospector-zmk-module"
expected = "ed98221f3b52b7066dbb10ba3af8a29150b93a5a"
actual = subprocess.check_output(["git", "-C", str(module), "rev-parse", "HEAD"], text=True).strip()
if actual != expected:
    raise SystemExit(f"Prospector revision changed: {actual}")
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
packed = (assets / "mascot.rgb565a8").read_bytes()
frame_size = 160 * 132 * 3
if len(packed) != 4 * frame_size:
    raise SystemExit("Unexpected mascot asset dimensions")
lines = ['#include <lvgl.h>']
for frame in range(4):
    data = packed[frame * frame_size:(frame + 1) * frame_size]
    lines.append(f'static const uint8_t mascot_data_{frame}[] __attribute__((aligned(4))) = {{')
    for offset in range(0, len(data), 24):
        lines.append(','.join(f'0x{b:02x}' for b in data[offset:offset + 24]) + ',')
    lines.append('};')
lines.append('const lv_image_dsc_t mascot_frames[4] = {')
for frame in range(4):
    lines.append('{.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565A8, '
                 '.w = 160, .h = 132, .stride = 320}, '
                 f'.data_size = {frame_size}, .data = mascot_data_{frame}' + '},')
lines.append('};')
(layout / "mascot_assets.c").write_text('\n'.join(lines) + '\n')
# Keep the actual default modifier widget, including its ordering and OS symbols.
# The new screen does not need the old roller/battery/output widgets or their fonts.
cmake = module / "boards/shields/prospector_adapter/CMakeLists.txt"
source = cmake.read_text()
old = 'file(GLOB layout_sources src/layouts/classic/*.c)'
assert source.count(old) == 1
source = source.replace(old, 'set(layout_sources src/layouts/classic/modifier_indicator.c '
                        'src/layouts/classic/mascot_assets.c)')
cmake.write_text(source)
print("Installed landscape mascot screen, transparent frames, and default modifiers")
