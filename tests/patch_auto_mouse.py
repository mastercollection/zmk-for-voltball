"""Filter resolved behavior presses/releases in the pinned ZMK dispatcher."""
from pathlib import Path
import subprocess

ZMK_REVISION = "4dd95d7840eb3d5a3f59bb29fbf1ab054715ef85"
ANCHOR = "    int err = behavior_keymap_binding_convert_central_state_dependent_params(&binding, event);"
HOOK = """#if IS_ENABLED(CONFIG_ZMK_INPUT_PROCESSOR_THRESHOLD_TEMP_LAYER)
    extern bool voltball_auto_mouse_filter(const struct device *behavior,
                                           const struct zmk_behavior_binding *binding,
                                           struct zmk_behavior_binding_event event, bool pressed);
    if (voltball_auto_mouse_filter(behavior, &binding, event, pressed)) {
        return ZMK_BEHAVIOR_OPAQUE;
    }
#endif
"""


def patch_source(source):
    if source.count(ANCHOR) != 1 or "voltball_auto_mouse_filter" in source:
        raise ValueError("Unexpected behavior dispatcher; review auto-mouse hook before building")
    return source.replace(ANCHOR, HOOK + ANCHOR)


def apply(zmk):
    zmk = Path(zmk)
    actual = subprocess.check_output(["git", "-C", str(zmk), "rev-parse", "HEAD"], text=True).strip()
    if actual != ZMK_REVISION:
        raise ValueError(f"ZMK revision changed: {actual}")
    path = zmk / "app/src/behavior.c"
    path.write_text(patch_source(path.read_text()))
    print("Patched ZMK: left non-modifier action exits auto mouse without emitting press/release")
