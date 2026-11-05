"""Pack the mascot sheets into LVGL I8 frames: a 256-entry ARGB8888 palette
followed by one byte per pixel. Index 0 is transparent.

Asset conversion only. Requires Pillow; the firmware build uses the packed data.
Frame order, mirrored by MASCOT_* in mascot_logic.h: the sitting idle pose, the
walking cycle, then the scooter cycle. Each frame in the output is prefixed by
its width and height so tests/patch_modules.py can emit the C descriptors.
"""
from pathlib import Path
import struct
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parent
HEIGHT = 132
WALK_FRAMES, RUN_FRAMES = 6, 5


def cells(name, count):
    sheet = Image.open(ROOT / name).convert("RGBA")
    width = sheet.width // count
    return [sheet.crop((i * width, 0, (i + 1) * width, sheet.height)) for i in range(count)]


def clean_outline(cell):
    """Tidy a frame cut from compressed video. Its black outline is about three
    pixels wide at source size; the two outermost opaque pixels are blends of
    that outline with the removed white background and would show as a grey
    fringe over the black screen, so they become plain black. Dark pixels
    anywhere turn pure black, which removes compression noise in the outline."""
    rgb, alpha = cell.convert("RGB"), cell.getchannel("A").point(lambda a: 255 if a else 0)
    interior = alpha.filter(ImageFilter.MinFilter(5))
    outline = rgb.convert("L").point(lambda l: 255 if l < 56 else 0)
    black = Image.new("RGB", cell.size, (0, 0, 0))
    rgb.paste(black, mask=Image.eval(interior, lambda v: 255 - v))
    rgb.paste(black, mask=outline)
    rgb.putalpha(alpha)
    return rgb


def fit_height(cell):
    return cell.resize((round(cell.width * HEIGHT / cell.height), HEIGHT), Image.Resampling.LANCZOS)


def pack(cell):
    # The mascot is drawn over the black screen with nothing behind it, so a
    # partly transparent edge pixel is stored pre-blended over black and opaque.
    black = Image.new("RGBA", cell.size, (0, 0, 0, 255))
    black.alpha_composite(cell)
    width = (cell.width + 3) // 4 * 4  # 4-byte aligned rows
    padded = Image.new("RGB", (width, cell.height), (0, 0, 0))
    padded.paste(black.convert("RGB"), (0, 0))
    alpha = Image.new("L", padded.size, 0)
    alpha.paste(cell.getchannel("A").point(lambda a: 255 if a else 0), (0, 0))
    indexed = padded.quantize(colors=255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    palette = indexed.getpalette()[:255 * 3]
    out = bytearray(struct.pack("<HH", width, cell.height))
    out += bytes(4)  # index 0: transparent, as lv_color32_t {blue, green, red, alpha}
    for i in range(255):
        r, g, b = palette[i * 3:i * 3 + 3] if i * 3 < len(palette) else (0, 0, 0)
        out += bytes((b, g, r, 255))
    for index, a in zip(indexed.get_flattened_data(), alpha.get_flattened_data()):
        out.append(index + 1 if a else 0)
    assert len(out) == 4 + 1024 + width * cell.height
    return out


frames = (
    # Sitting pose from the approved four-pose sheet: 160x213, then its middle 132 rows.
    [cells("source-sheet.png", 4)[0].resize((160, 213), Image.Resampling.LANCZOS).crop((0, 40, 160, 172))]
    + [fit_height(clean_outline(cell)) for cell in cells("walk-sheet.png", WALK_FRAMES)]
    + [fit_height(clean_outline(cell)) for cell in cells("scooter-sheet.png", RUN_FRAMES)]
)
packed = b"".join(pack(frame) for frame in frames)
(ROOT / "mascot.i8").write_bytes(packed)
print(f"Packed {len(frames)} I8 frames: {len(packed)} bytes")
