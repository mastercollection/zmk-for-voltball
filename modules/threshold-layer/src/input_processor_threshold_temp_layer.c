/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_threshold_temp_layer
#include <errno.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <drivers/input_processor.h>
#include <zmk/keymap.h>
#include <zmk/keys.h>
#include <zmk/matrix.h>
#include <zmk/behavior.h>
#include <zmk/virtual_key_position.h>
#include "auto_mouse_state.h"

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
             "Voltball auto mouse supports one processor instance");

struct threshold_config {
    uint32_t threshold;
    uint32_t idle_ms;
    uint8_t mouse_layer;
};
struct threshold_data {
    struct k_mutex lock;
    struct auto_mouse_state layers[ZMK_KEYMAP_LAYERS_LEN];
};

/* Match pinned ZMK combo.c ordering: ascending key count, then devicetree order. */
#if DT_HAS_COMPAT_STATUS_OKAY(zmk_combos)
struct exit_combo {
    const uint32_t *positions;
    size_t count;
};
#define EXIT_COMBO(n, length) \
    COND_CODE_1(IS_EQ(DT_PROP_LEN(n, key_positions), length), \
                ({.positions = (const uint32_t[])DT_PROP(n, key_positions), \
                  .count = DT_PROP_LEN(n, key_positions)},), ())
#define EXIT_COMBOS_BY_LENGTH(length, unused) \
    DT_FOREACH_CHILD_VARGS(DT_INST(0, zmk_combos), EXIT_COMBO, length)
static const struct exit_combo exit_combos[] = {
    LISTIFY(20, EXIT_COMBOS_BY_LENGTH, (), 0)};
#endif

/* Asked on each exit-key non-modifier press, whether or not auto mouse was on.
 * Returning false marks a pointer control (&ptr): it neither types nor exits. */
static bool (*typing_callback)(const struct device *behavior);

void voltball_auto_mouse_set_typing_callback(bool (*callback)(const struct device *behavior)) {
    typing_callback = callback;
}

/* Keymap bindings name their behavior; compare names instead of a device lookup. */
#define MATCH_NAME(n) || strcmp(name, DEVICE_DT_NAME(n)) == 0
static bool is_transparent_name(const char *name) {
    return name != NULL && (false DT_FOREACH_STATUS_OKAY(zmk_behavior_transparent, MATCH_NAME));
}
#undef MATCH_NAME

#define MATCH_DEVICE(n) || behavior == DEVICE_DT_GET(n)

static bool is_deferred_behavior(const struct device *behavior) {
    /* Transparent bindings must fall through; hold-taps must resolve first. */
    return false DT_FOREACH_STATUS_OKAY(zmk_behavior_transparent, MATCH_DEVICE)
        DT_FOREACH_STATUS_OKAY(zmk_behavior_hold_tap, MATCH_DEVICE);
}

static bool is_modifier_behavior(const struct device *behavior, uint32_t encoded) {
    bool key_behavior = false DT_FOREACH_STATUS_OKAY(zmk_behavior_key_press, MATCH_DEVICE)
        DT_FOREACH_STATUS_OKAY(zmk_behavior_sticky_key, MATCH_DEVICE);
    uint16_t page = ZMK_HID_USAGE_PAGE(encoded);
    uint16_t key = ZMK_HID_USAGE_ID(encoded);
    if (page == 0) {
        page = HID_USAGE_KEY;
    }
    return key_behavior && is_mod(page, key);
}
#undef MATCH_DEVICE

/* While the mouse layer is active, a key exits when it is &trans there, so
 * anything placed on it (buttons, &ptr) keeps working. While it is off, every
 * key is typing. A combo exits when any of its keys would.
 */
static bool is_exit_key(uint8_t layer, uint32_t position) {
    if (!zmk_keymap_layer_active(zmk_keymap_layer_index_to_id(layer))) {
        return true;
    }
    if (position < ZMK_KEYMAP_LEN) {
        const struct zmk_behavior_binding *binding = zmk_keymap_get_layer_binding_at_idx(
            zmk_keymap_layer_index_to_id(layer), position);
        return binding != NULL && is_transparent_name(binding->behavior_dev);
    }
#if DT_HAS_COMPAT_STATUS_OKAY(zmk_combos)
    uint32_t combo_start = ZMK_VIRTUAL_KEY_POSITION_COMBO(0);
    if (position >= combo_start && position - combo_start < ARRAY_SIZE(exit_combos)) {
        const struct exit_combo *combo = &exit_combos[position - combo_start];
        for (size_t i = 0; i < combo->count; i++) {
            if (combo->positions[i] < ZMK_KEYMAP_LEN && is_exit_key(layer, combo->positions[i])) {
                return true;
            }
        }
    }
#endif
    return false;
}

/* Called before behavior dispatch on both press and release. Never consumes:
 * an exit key ends auto mouse and still types, as on QMK Keyball.
 */
void voltball_auto_mouse_filter(const struct device *behavior,
                                const struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event, bool pressed) {
    if (!pressed || is_deferred_behavior(behavior) ||
        is_modifier_behavior(behavior, binding->param1)) {
        return;
    }
    const struct device *dev = DEVICE_DT_INST_GET(0);
    const struct threshold_config *cfg = dev->config;
    struct threshold_data *data = dev->data;
    k_mutex_lock(&data->lock, K_FOREVER);
    bool exit = is_exit_key(cfg->mouse_layer, event.position);
    k_mutex_unlock(&data->lock);
    /* Ask outside the lock: the callback takes pointer mode's own lock. */
    if (!exit || (typing_callback && !typing_callback(behavior))) {
        return;
    }
    k_mutex_lock(&data->lock, K_FOREVER);
    int64_t now = k_uptime_get();
    for (int i = 0; i < ZMK_KEYMAP_LAYERS_LEN; i++) {
        zmk_keymap_layer_id_t id = zmk_keymap_layer_index_to_id(i);
        bool active = data->layers[i].active && zmk_keymap_layer_active(id);
        auto_mouse_typing(&data->layers[i], now, cfg->idle_ms);
        if (active) {
            zmk_keymap_layer_deactivate(id, false);
        }
    }
    k_mutex_unlock(&data->lock);
}

static int threshold_handle_event(const struct device *dev, struct input_event *event,
                                  uint32_t layer, uint32_t timeout,
                                  struct zmk_input_processor_state *state) {
    ARG_UNUSED(state);
    const struct threshold_config *cfg = dev->config;
    /* The cell must match mouse-layer, which the key filter reads before any motion. */
    if (layer != cfg->mouse_layer || timeout != 0) {
        return -EINVAL;
    }
    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    struct threshold_data *data = dev->data;
    k_mutex_lock(&data->lock, K_FOREVER);
    struct auto_mouse_state *s = &data->layers[layer];
    zmk_keymap_layer_id_t id = zmk_keymap_layer_index_to_id(layer);
    if (s->active && !zmk_keymap_layer_active(id)) {
        s->active = false;
        s->distance = 0;
    }
    if (auto_mouse_motion(s, event->value, k_uptime_get(), cfg->threshold)) {
        zmk_keymap_layer_activate(id, false);
    }
    k_mutex_unlock(&data->lock);
    return ZMK_INPUT_PROC_CONTINUE;
}
static int threshold_init(const struct device *dev) {
    struct threshold_data *data = dev->data;
    k_mutex_init(&data->lock);
    return 0;
}
static const struct zmk_input_processor_driver_api threshold_api = {
    .handle_event = threshold_handle_event,
};
BUILD_ASSERT(DT_INST_PROP(0, activation_threshold) >= 0, "Threshold must be nonnegative");
BUILD_ASSERT(DT_INST_PROP(0, require_prior_idle_ms) >= 0, "Idle time must be nonnegative");
BUILD_ASSERT(DT_INST_PROP(0, mouse_layer) < ZMK_KEYMAP_LAYERS_LEN, "mouse-layer out of range");
static const struct threshold_config config = {
    .threshold = DT_INST_PROP(0, activation_threshold),
    .idle_ms = DT_INST_PROP(0, require_prior_idle_ms),
    .mouse_layer = DT_INST_PROP(0, mouse_layer),
};
static struct threshold_data data;
DEVICE_DT_INST_DEFINE(0, threshold_init, NULL, &data, &config, POST_KERNEL,
                      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &threshold_api);
