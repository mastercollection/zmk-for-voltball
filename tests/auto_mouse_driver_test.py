"""Compile the actual processor against host stubs; not a Zephyr firmware build."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[1]
src = ROOT / "modules/threshold-layer/src"
config_text = (ROOT / "config/boards/shields/voltball/voltball-trackball-processing.dtsi").read_text()
positions = re.search(r"typing-positions\s*=\s*<(.*?)>", config_text, re.S)[1].split()
PREAMBLE = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define BUILD_ASSERT(c, m) _Static_assert(c, m)
#define ARG_UNUSED(x) (void)(x)
#define K_FOREVER 0
#define ZMK_KEYMAP_LAYERS_LEN 7
#define ZMK_INPUT_PROC_CONTINUE 0
#define INPUT_EV_REL 2
#define INPUT_REL_X 0
#define INPUT_REL_Y 1
#define HID_USAGE_KEY 7
#define ZMK_HID_USAGE_PAGE(k) ((k) >> 16)
#define ZMK_HID_USAGE_ID(k) ((k) & 0xffff)
#define ZMK_VIRTUAL_KEY_POSITION_COMBO(i) (42 + (i))
#define ZMK_COMBOS_LEN 2
#define DT_NUM_INST_STATUS_OKAY(x) 1
#define DT_HAS_COMPAT_STATUS_OKAY(x) 1
#define DT_INST(n, compat) n
#define DT_INST_PROP(n, p) prop_##p
#define prop_typing_positions {POSITIONS}
#define prop_activation_threshold 100
#define prop_require_prior_idle_ms 200
#define DT_PROP_LEN(n, p) 2
#define DT_PROP(n, p) props_##n
#define props_left {2, 3}
#define props_right {6, 7}
#define IS_EQ(a, b) 1
#define UNPAREN(...) __VA_ARGS__
#define COND_CODE_1(flag, yes, no) UNPAREN yes
#define DT_FOREACH_CHILD_VARGS(node, fn, len) fn(left, len) fn(right, len)
#define LISTIFY(n, fn, sep, arg) fn(2, arg)
#define DEVICE_DT_INST_DEFINE(...)
#define DEVICE_DT_INST_GET(n) (&mock_device)
#define DEVICE_DT_GET(n) (&device_##n)
#define DT_FOREACH_STATUS_OKAY(compat, fn) nodes_##compat(fn)
#define nodes_zmk_behavior_transparent(fn) fn(trans)
#define nodes_zmk_behavior_hold_tap(fn) fn(mt)
#define nodes_zmk_behavior_key_press(fn) fn(kp)
#define nodes_zmk_behavior_sticky_key(fn) fn(sk)
typedef uint8_t zmk_keymap_layer_id_t;
struct k_mutex { int unused; };
static int k_mutex_init(struct k_mutex *m) { (void)m; return 0; }
static int k_mutex_lock(struct k_mutex *m, int wait) { (void)m; (void)wait; return 0; }
static int k_mutex_unlock(struct k_mutex *m) { (void)m; return 0; }
static int64_t fake_now;
static int64_t k_uptime_get(void) { return fake_now; }
struct device { const void *config; void *data; };
static struct device mock_device;
static struct device device_trans, device_mt, device_kp, device_sk;
static struct device device_scroll, device_click, device_layer, device_none;
struct zmk_behavior_binding { const char *behavior_dev; uint32_t param1, param2; };
struct zmk_behavior_binding_event { uint32_t position; };
struct input_event { int type, code; int32_t value; };
struct zmk_input_processor_state { int unused; };
struct zmk_input_processor_driver_api {
    int (*handle_event)(const struct device *, struct input_event *, uint32_t,
                        uint32_t, struct zmk_input_processor_state *);
};
static bool active_layers[7], locked_layers[7];
static uint8_t zmk_keymap_layer_index_to_id(uint32_t i) { return (uint8_t)i; }
static bool zmk_keymap_layer_active(uint8_t i) { return active_layers[i]; }
static int zmk_keymap_layer_activate(uint8_t i, bool lock) {
    active_layers[i] = true; if (lock) locked_layers[i] = true; return 0;
}
static int zmk_keymap_layer_deactivate(uint8_t i, bool lock) {
    if (!locked_layers[i] || lock) active_layers[i] = false;
    return 0;
}
static bool is_mod(uint16_t page, uint32_t key) {
    return page == HID_USAGE_KEY && key >= 0xe0 && key <= 0xe7;
}
"""
TEST = (ROOT / "tests/auto_mouse_driver_cases.h").read_text()
source = src.joinpath("input_processor_threshold_temp_layer.c").read_text(encoding="utf-8-sig")
source = re.sub(r"^#include <(?:zephyr/|drivers/|zmk/).*\n", "", source, flags=re.M)
with tempfile.TemporaryDirectory(prefix="voltball-auto-mouse-", dir=ROOT / "tests") as tmp:
    tmp = Path(tmp)
    test = tmp / "driver_test.c"
    test.write_text(PREAMBLE.replace("POSITIONS", ", ".join(positions)) + source + TEST)
    compiler = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("CC", "cc")
    output = tmp / ("driver_test.exe" if os.name == "nt" else "driver_test")
    if Path(compiler).stem.lower() == "cl":
        args = [compiler, "/nologo", "/std:c11", "/W4", "/WX", "/I" + str(src),
                str(test), "/Fo:" + str(tmp / "driver_test.obj"), "/Fe:" + str(output)]
    else:
        args = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(src),
                str(test), "-o", str(output)]
    subprocess.run(args, check=True, cwd=tmp)
    subprocess.run([str(output)], check=True)

