/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_voltball_pointer
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <drivers/input_processor.h>
#include <zmk/behavior.h>
#include <dt-bindings/voltball/pointer_mode.h>
#include "pointer_mode_state.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
             "Voltball pointer mode supports one processor instance");

#define POINTER_SCALES DT_INST_PROP(0, pointer_scales)
#define SCROLL_SCALES DT_INST_PROP(0, scroll_scales)
BUILD_ASSERT(DT_INST_PROP_LEN(0, pointer_scales) % 2 == 0, "pointer-scales are mul/div pairs");
BUILD_ASSERT(DT_INST_PROP_LEN(0, scroll_scales) % 2 == 0, "scroll-scales are mul/div pairs");

static const int32_t pointer_scales[] = POINTER_SCALES;
static const int32_t scroll_scales[] = SCROLL_SCALES;
#define POINTER_STEPS (ARRAY_SIZE(pointer_scales) / 2)
#define SCROLL_STEPS (ARRAY_SIZE(scroll_scales) / 2)
#define POINTER_DEFAULT DT_INST_PROP(0, pointer_default_index)
#define SCROLL_DEFAULT DT_INST_PROP(0, scroll_default_index)
BUILD_ASSERT(POINTER_DEFAULT < POINTER_STEPS, "pointer-default-index out of range");
BUILD_ASSERT(SCROLL_DEFAULT < SCROLL_STEPS, "scroll-default-index out of range");
BUILD_ASSERT(POINTER_STEPS <= UINT8_MAX && SCROLL_STEPS <= UINT8_MAX, "Too many steps");

#define PTR_BEHAVIOR DT_INST(0, zmk_behavior_voltball_pointer)
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(zmk_behavior_voltball_pointer) == 1,
             "Voltball pointer mode supports one &ptr behavior");
DEVICE_DT_DECLARE(PTR_BEHAVIOR);

static const bool scroll_invert_y = DT_INST_PROP(0, scroll_invert_y);

static K_MUTEX_DEFINE(ptr_lock);
static struct ptr_state state = {
    .pointer_step = POINTER_DEFAULT,
    .scroll_step = SCROLL_DEFAULT,
};

static struct ptr_ratio ratio_at(const int32_t *scales, uint8_t step) {
    return (struct ptr_ratio){.mul = scales[step * 2], .div = scales[step * 2 + 1]};
}

/* Only the steps persist; scroll mode always starts off. */
struct ptr_saved {
    uint8_t pointer_step;
    uint8_t scroll_step;
};

#if IS_ENABLED(CONFIG_SETTINGS)
#define PTR_SETTINGS_KEY "voltball_ptr/steps"

static void ptr_save_work_handler(struct k_work *work) {
    k_mutex_lock(&ptr_lock, K_FOREVER);
    struct ptr_saved saved = {state.pointer_step, state.scroll_step};
    k_mutex_unlock(&ptr_lock);
    settings_save_one(PTR_SETTINGS_KEY, &saved, sizeof(saved));
}
static K_WORK_DELAYABLE_DEFINE(ptr_save_work, ptr_save_work_handler);

static int ptr_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    if (!settings_name_steq(name, "steps", NULL) || len != sizeof(struct ptr_saved)) {
        return -ENOENT;
    }
    struct ptr_saved saved;
    int err = read_cb(cb_arg, &saved, sizeof(saved));
    if (err < 0) {
        return err;
    }
    /* Ignore values saved with a longer step table. */
    k_mutex_lock(&ptr_lock, K_FOREVER);
    if (saved.pointer_step < POINTER_STEPS) {
        state.pointer_step = saved.pointer_step;
    }
    if (saved.scroll_step < SCROLL_STEPS) {
        state.scroll_step = saved.scroll_step;
    }
    k_mutex_unlock(&ptr_lock);
    return 0;
}
SETTINGS_STATIC_HANDLER_DEFINE(voltball_ptr, "voltball_ptr", NULL, ptr_settings_set, NULL, NULL);

static void ptr_schedule_save(void) {
    k_work_reschedule(&ptr_save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
}
#else
static void ptr_schedule_save(void) {}
#endif

static int ptr_handle_event(const struct device *dev, struct input_event *event, uint32_t stage,
                            uint32_t unused, struct zmk_input_processor_state *proc_state) {
    ARG_UNUSED(dev);
    ARG_UNUSED(unused);
    ARG_UNUSED(proc_state);
    if (stage > PTR_STAGE_SPEED) {
        return -EINVAL;
    }
    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    bool is_x = event->code == INPUT_REL_X;
    k_mutex_lock(&ptr_lock, K_FOREVER);
    if (ptr_scroll_active(&state)) {
        /* MODE runs before auto mouse, so scrolling never latches the mouse layer. */
        if (stage == PTR_STAGE_MODE) {
            int32_t value = (!is_x && scroll_invert_y) ? -event->value : event->value;
            event->code = is_x ? INPUT_REL_HWHEEL : INPUT_REL_WHEEL;
            event->value = ptr_scale(value, ratio_at(scroll_scales, state.scroll_step),
                                     &state.remainder[is_x ? PTR_SCROLL_X : PTR_SCROLL_Y]);
        }
    } else if (stage == PTR_STAGE_SPEED) {
        event->value = ptr_scale(event->value, ratio_at(pointer_scales, state.pointer_step),
                                 &state.remainder[is_x ? PTR_POINTER_X : PTR_POINTER_Y]);
    }
    k_mutex_unlock(&ptr_lock);
    return ZMK_INPUT_PROC_CONTINUE;
}

#if IS_ENABLED(CONFIG_ZMK_INPUT_PROCESSOR_THRESHOLD_TEMP_LAYER)
void voltball_auto_mouse_set_typing_callback(void (*callback)(const struct device *behavior));

/* Left-hand typing ends a latched drag scroll, like it ends auto mouse.
 * &ptr itself is exempt so a left-hand toggle key can switch scroll off. */
static void ptr_on_typing(const struct device *behavior) {
    if (behavior == DEVICE_DT_GET(PTR_BEHAVIOR)) {
        return;
    }
    k_mutex_lock(&ptr_lock, K_FOREVER);
    if (ptr_end_toggle(&state)) {
        LOG_DBG("scroll toggle cleared by typing");
    }
    k_mutex_unlock(&ptr_lock);
}

static int ptr_init(const struct device *dev) {
    ARG_UNUSED(dev);
    voltball_auto_mouse_set_typing_callback(ptr_on_typing);
    return 0;
}
#define PTR_INIT ptr_init
#else
#define PTR_INIT NULL
#endif

static const struct zmk_input_processor_driver_api ptr_processor_api = {
    .handle_event = ptr_handle_event,
};
DEVICE_DT_INST_DEFINE(0, PTR_INIT, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                      &ptr_processor_api);

/* &ptr behavior. Runs on the central, where the processor also lives. */

static int ptr_binding_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    bool save = false;
    k_mutex_lock(&ptr_lock, K_FOREVER);
    bool scroll = ptr_scroll_active(&state);
    uint8_t *step = scroll ? &state.scroll_step : &state.pointer_step;
    uint8_t count = scroll ? SCROLL_STEPS : POINTER_STEPS;
    switch (binding->param1) {
    case PTR_SPD_UP:
        save = ptr_step(step, 1, count);
        break;
    case PTR_SPD_DN:
        save = ptr_step(step, -1, count);
        break;
    case PTR_SPD_RST: {
        uint8_t initial = scroll ? SCROLL_DEFAULT : POINTER_DEFAULT;
        save = *step != initial;
        *step = initial;
        break;
    }
    case PTR_SCRL_TOG:
        state.scroll_toggled = !state.scroll_toggled;
        break;
    case PTR_SCRL_MO:
        state.scroll_held++;
        break;
    default:
        k_mutex_unlock(&ptr_lock);
        return -ENOTSUP;
    }
    ptr_clear_remainders(&state);
    LOG_DBG("pointer step %d scroll step %d scroll %d", state.pointer_step, state.scroll_step,
            ptr_scroll_active(&state));
    k_mutex_unlock(&ptr_lock);
    if (save) {
        ptr_schedule_save();
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

static int ptr_binding_released(struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    if (binding->param1 == PTR_SCRL_MO) {
        k_mutex_lock(&ptr_lock, K_FOREVER);
        if (state.scroll_held > 0) {
            state.scroll_held--;
        }
        ptr_clear_remainders(&state);
        k_mutex_unlock(&ptr_lock);
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
static const struct behavior_parameter_value_metadata ptr_param_values[] = {
    {.display_name = "Speed Up", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = PTR_SPD_UP},
    {.display_name = "Speed Down", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = PTR_SPD_DN},
    {.display_name = "Speed Reset", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = PTR_SPD_RST},
    {.display_name = "Scroll Toggle", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
     .value = PTR_SCRL_TOG},
    {.display_name = "Scroll Hold", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
     .value = PTR_SCRL_MO},
};
static const struct behavior_parameter_metadata_set ptr_param_sets[] = {{
    .param1_values = ptr_param_values,
    .param1_values_len = ARRAY_SIZE(ptr_param_values),
}};
static const struct behavior_parameter_metadata ptr_metadata = {
    .sets_len = ARRAY_SIZE(ptr_param_sets),
    .sets = ptr_param_sets,
};
#endif

static const struct behavior_driver_api ptr_behavior_api = {
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
    .binding_pressed = ptr_binding_pressed,
    .binding_released = ptr_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &ptr_metadata,
#endif
};
BEHAVIOR_DT_DEFINE(PTR_BEHAVIOR, NULL, NULL, NULL, NULL, POST_KERNEL,
                   CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &ptr_behavior_api);
