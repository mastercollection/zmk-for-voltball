"""Check the behavior-dispatch patch without modifying the vendored ZMK."""
from pathlib import Path
from patch_auto_mouse import patch_source, ANCHOR, HOOK

ROOT = Path(__file__).resolve().parents[1]
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
print("PASS: guarded behavior filter")
