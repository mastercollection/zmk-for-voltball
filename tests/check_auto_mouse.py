"""Check configuration and behavior filter without modifying the vendored ZMK."""
from pathlib import Path
import re
from patch_auto_mouse import patch_source, ANCHOR, HOOK

ROOT = Path(__file__).resolve().parents[1]
config = (ROOT / "config/boards/shields/voltball/voltball-trackball-processing.dtsi").read_text()
positions = list(map(int, re.search(r"typing-positions\s*=\s*<(.*?)>", config, re.S)[1].split()))
layout = (ROOT / "config/boards/shields/voltball/voltball-layout.dtsi").read_text()
columns = [int(col) for _, col in re.findall(r"RC\((\d+),(\d+)\)", layout)]
assert positions == [i for i, col in enumerate(columns) if col < 6]
assert "<&zip_threshold_temp_layer 5 0>" in config
assert "require-prior-idle-ms = <200>" in config
schemas = [ROOT / "dts/bindings/zmk,input-processor-threshold-temp-layer.yaml",
           ROOT / "modules/threshold-layer/dts/bindings/zmk,input-processor-threshold-temp-layer.yaml"]
assert schemas[0].read_text() == schemas[1].read_text()

# Exact upstream body is available locally; CI supplies its fetched source later.
source_path = ROOT.parent / "sources/zmk/app/src/behavior.c"
if source_path.exists():
    original = source_path.read_text()
else:
    original = "void press() {\n" + ANCHOR + "\n}\n"
patched = patch_source(original)
assert patched.replace(HOOK, "") == original
assert patched.index("if (voltball_auto_mouse_filter(behavior, &binding, event, pressed))") < patched.index(ANCHOR)
assert "return ZMK_BEHAVIOR_OPAQUE;" in HOOK
for invalid in (patched, "", original + ANCHOR):
    try:
        patch_source(invalid)
    except ValueError:
        pass
    else:
        raise AssertionError("Patch must reject drift or duplicate application")
print("PASS: physical left half, persistent mode, matching schemas, guarded behavior filter")
