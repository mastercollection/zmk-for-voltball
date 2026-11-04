"""Check configuration and behavior filter without modifying the vendored ZMK."""
from pathlib import Path
import re
from patch_auto_mouse import patch_source, ANCHOR, HOOK

ROOT = Path(__file__).resolve().parents[1]
config = (ROOT / "config/boards/shields/voltball/voltball-trackball-processing.dtsi").read_text()
# Exit keys come from the MOUSE layer's &trans bindings, not a position list.
assert "typing-positions" not in config
driver = (ROOT / "modules/threshold-layer/src/input_processor_threshold_temp_layer.c").read_text()
assert "zmk_keymap_get_layer_binding_at_idx" in driver
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
assert patched.index("voltball_auto_mouse_filter(behavior, &binding, event, pressed);") < patched.index(ANCHOR)
assert "return" not in HOOK
for invalid in (patched, "", original + ANCHOR):
    try:
        patch_source(invalid)
    except ValueError:
        pass
    else:
        raise AssertionError("Patch must reject drift or duplicate application")
print("PASS: &trans exit keys, persistent mode, matching schemas, guarded behavior filter")
