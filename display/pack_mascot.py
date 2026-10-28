"""Pack the approved transparent four-pose sheet into LVGL RGB565A8 frames.

Asset conversion only. Requires Pillow; the firmware build uses the packed data.
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent
sheet = Image.open(ROOT / "source-sheet.png").convert("RGBA")
width, height = 160, 132
out = bytearray()
for frame in range(4):
    cell = sheet.crop((frame * sheet.width // 4, 0,
                       (frame + 1) * sheet.width // 4, sheet.height))
    cell = cell.resize((160, 213), Image.Resampling.LANCZOS).crop((0, 40, 160, 172))
    rgb, alpha = bytearray(), bytearray()
    for r, g, b, a in cell.getdata():
        value = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)
        rgb.extend(value.to_bytes(2, "little"))
        alpha.append(a)
    out.extend(rgb + alpha)
(ROOT / "mascot.rgb565a8").write_bytes(out)
assert len(out) == 4 * width * height * 3
print(f"Packed four transparent {width}x{height} frames: {len(out)} bytes")
