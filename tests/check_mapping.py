"""Static migration checks; not a firmware build or a hardware test."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
def read(path):
    text = (ROOT / path).read_text(encoding="utf-8-sig")
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)

original = read("tests/fixtures/totem-b4b2073.keymap")
current = read("config/voltball.keymap")
# Explicit approved destination for each source position, including the two outer keys.
destination = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,
               32,20,21,22,23,24,27,28,29,30,31,41,35,36,37,38,39,40]
def bindings(text):
    return [[" ".join(v.split()) for v in re.findall(r"&[^&]+", block)]
            for block in re.findall(r"bindings\s*=\s*<(.*?)>;", text, re.S)]
source_layers = bindings(original.split('compatible = "zmk,keymap";', 1)[1])
target_layers = bindings(current.split('compatible = "zmk,keymap";', 1)[1])
assert len(source_layers) == 6 and len(target_layers) == 7
for layer, source in enumerate(source_layers):
    assert len(source) == 38 and len(target_layers[layer]) == 42
    for src, dst in enumerate(destination):
        assert source[src] == target_layers[layer][dst], (layer, src, dst)
for pos, expected in {25: "&mo 5", 26: "&mo 6", 33: "&studio_unlock", 34: "&none"}.items():
    assert target_layers[0][pos] == expected
assert target_layers[6] == ["&trans"] * 42
for a, b in zip(re.findall(r"key-positions = <(.*?)>", original),
                re.findall(r"key-positions = <(.*?)>", current)):
    assert [destination[int(x)] for x in a.split()] == [int(x) for x in b.split()]
assert len(re.findall(r"key-positions", current)) == 9
assert bindings(original.split("    keymap {", 1)[0]) == bindings(current.split("    keymap {", 1)[0])
assert re.findall(r"timeout-ms = <(\d+)>", current) == ["20"] * 9
for name in ["mt", "lt"]:
    pattern = r"&" + name + r"\s*\{(.*?)\};"
    assert re.sub(r"\s", "", re.search(pattern, original, re.S)[1]) == re.sub(
        r"\s", "", re.search(pattern, current, re.S)[1])
layout = read("config/boards/shields/voltball/voltball-layout.dtsi")
assert len(re.findall(r"&key_physical_attrs", layout)) == 42
assert len(re.findall(r"RC\(", layout)) == 42
print("PASS: 228 bindings, 9 combos, hold-tap timing, approved extra keys, 42 positions")
