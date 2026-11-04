"""Structural keymap checks. Bindings are edited freely (keymap-editor)."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def read(path):
    text = (ROOT / path).read_text(encoding="utf-8-sig")
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)

def bindings(text):
    return [[" ".join(v.split()) for v in re.findall(r"&[^&]+", block)]
            for block in re.findall(r"bindings\s*=\s*<(.*?)>;", text, re.S)]

current = read("config/voltball.keymap")
keymap = current.split('compatible = "zmk,keymap";', 1)[1]
layers = bindings(keymap)
assert layers and all(len(layer) == 42 for layer in layers)
# Input processors refer to layers by index; keep them on the named layers.
names = re.findall(r'display-name\s*=\s*"([^"]+)"', keymap)
assert len(names) == len(layers), "every layer needs a display-name"
# Scroll is a &ptr mode, not a layer; the S+D style combo uses the hold-tap.
assert "&ptr_ht PTR_SCRL_MO PTR_SCRL_TOG" in current and "&mo 7" not in current
# Combo positions must exist on the 42-key layout.
for positions in re.findall(r"key-positions\s*=\s*<(.*?)>", current):
    assert all(0 <= int(p) < 42 for p in positions.split())
layout = read("config/boards/shields/voltball/voltball-layout.dtsi")
assert len(re.findall(r"&key_physical_attrs", layout)) == 42
assert len(re.findall(r"RC\(", layout)) == 42
processing = read("config/boards/shields/voltball/voltball-trackball-processing.dtsi")
assert "scroller" not in processing
chain = re.search(r"input-processors\s*=\s*(<&zip_pointer_mode PTR_STAGE_MODE>.*?);", processing, re.S)[1]
assert re.findall(r"<&(\w+)", chain) == ["zip_pointer_mode", "zip_threshold_temp_layer", "zip_pointer_mode"]
mouse = names.index("MOUSE")
assert f"&zip_threshold_temp_layer {mouse} 0>" in processing, "auto mouse cell must be the MOUSE index"
assert re.search(rf"mouse-layer\s*=\s*<{mouse}>", processing), "mouse-layer must be the MOUSE index"
assert re.search(rf"arrows\s*\{{\s*layers\s*=\s*<{names.index('NAVI')}>", processing), "arrows must be on NAVI"
print("PASS: layers of 42 keys, combo positions in range, MOUSE/NAVI indices match, scroll via &ptr")

# Shift home-row keys skip prior-idle so Shift right after a letter still holds.
for name in ("hsl", "hsr"):
    block = re.search(rf"{name}: \w+ \{{(.*?)\}};", current, re.S)
    assert block and "require-prior-idle-ms" not in block.group(1), f"{name} must not use prior-idle"
assert "&hsl LSHFT F" in current and "&hsr RSHFT J" in current
