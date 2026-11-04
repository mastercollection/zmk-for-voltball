"""Structural keymap checks. Bindings are edited freely (keymap-editor, Studio)."""
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
layers = bindings(current.split('compatible = "zmk,keymap";', 1)[1])
assert len(layers) == 7 and all(len(layer) == 42 for layer in layers)
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
assert "&zip_threshold_temp_layer 5 0>" in processing
print("PASS: 7 layers of 42 keys, combo positions in range, scroll via &ptr")
