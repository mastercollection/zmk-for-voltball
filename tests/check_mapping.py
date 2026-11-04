"""Ensure the Prospector integration preserves the main branch key bindings."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def read(path):
    text = (ROOT / path).read_text(encoding="utf-8-sig")
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)

def bindings(text):
    return [[" ".join(v.split()) for v in re.findall(r"&[^&]+", block)]
            for block in re.findall(r"bindings\s*=\s*<(.*?)>;", text, re.S)]

reference = read("tests/fixtures/main-2eda3c9.keymap")
current = read("config/voltball.keymap")
# SYM symbols moved to the left hand; everything else matches main 2eda3c9.
SYM = 10 + 1
sym_left = (["&kp LS(DQT)", "&kp LS(AMPS)", "&kp LS(STAR)", "&kp LS(TILDE)", "&kp APOS"] + ["&trans"] * 5
            + ["&kp GRAVE", "&kp LS(DLLR)", "&kp LS(PRCNT)", "&kp LS(CARET)", "&kp EQUAL"] + ["&trans"] * 5
            + ["&kp LS(TILDE)", "&kp LS(EXCL)", "&kp LS(AT)", "&kp LS(POUND)", "&kp LS(PLUS)"] + ["&trans"] * 7)
expected = bindings(reference)
expected[SYM] = sym_left + expected[SYM][32:]
assert bindings(current) == expected, "Main bindings or combos changed"
layers = bindings(current.split('compatible = "zmk,keymap";', 1)[1])
assert len(layers) == 8 and all(len(layer) == 42 for layer in layers)
for pattern in (r"key-positions\s*=\s*<(.*?)>", r"timeout-ms\s*=\s*<(.*?)>"):
    assert re.findall(pattern, current) == re.findall(pattern, reference)
for name in ("mt", "lt"):
    pattern = r"&" + name + r"\s*\{(.*?)\};"
    assert re.sub(r"\s", "", re.search(pattern, current, re.S)[1]) == re.sub(
        r"\s", "", re.search(pattern, reference, re.S)[1])
layout = read("config/boards/shields/voltball/voltball-layout.dtsi")
assert len(re.findall(r"&key_physical_attrs", layout)) == 42
assert len(re.findall(r"RC\(", layout)) == 42
processing = read("config/boards/shields/voltball/voltball-trackball-processing.dtsi")
assert re.search(r"scroller\s*\{\s*layers\s*=\s*<7>", processing)
assert "&zip_threshold_temp_layer 5 0>" in processing
print("PASS: main 2eda3c9 bindings + left-hand SYM, 8 layers, 10 combos, hold-tap timings, SCROLL 7")
