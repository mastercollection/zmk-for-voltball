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
assert len(layers) == 8 and all(len(layer) == 42 for layer in layers)
# Combo positions must exist on the 42-key layout.
for positions in re.findall(r"key-positions\s*=\s*<(.*?)>", current):
    assert all(0 <= int(p) < 42 for p in positions.split())
layout = read("config/boards/shields/voltball/voltball-layout.dtsi")
assert len(re.findall(r"&key_physical_attrs", layout)) == 42
assert len(re.findall(r"RC\(", layout)) == 42
processing = read("config/boards/shields/voltball/voltball-trackball-processing.dtsi")
assert re.search(r"scroller\s*\{\s*layers\s*=\s*<7>", processing)
assert "&zip_threshold_temp_layer 5 0>" in processing
print("PASS: 8 layers of 42 keys, combo positions in range, SCROLL 7")
