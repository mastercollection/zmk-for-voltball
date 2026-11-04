/* Landscape mascot screen for the pinned Prospector module, LVGL 9. */
#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/led.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_central_status_changed.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/caps_word_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/hid_indicators.h>
#include <zmk/wpm.h>
#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include "modifier_indicator.h"
#include "mascot_logic.h"
#if IS_ENABLED(CONFIG_ZMK_VOLTBALL_POINTER_MODE)
#include <voltball/pointer_mode.h>
#endif

extern const lv_image_dsc_t mascot_frames[4];
static struct zmk_widget_modifier_indicator modifier_widget;
static lv_obj_t *pet, *layer_label, *speed_label;
/* Indicator boxes left to right after the layer name: Scroll, Caps, NumLock. */
enum { IND_SCROLL, IND_CAPS, IND_NUM, IND_COUNT };
static lv_obj_t *lock_boxes[IND_COUNT], *lock_glyphs[IND_COUNT];
static lv_obj_t *battery_labels[2], *battery_bars[2];
static uint32_t previous_colors[IND_COUNT];
/* A speed key press shows the new ratio in the WPM slot for this long. */
#define SPEED_FLASH_MS 1000
static uint32_t speed_flash_until, previous_speed_presses;
static bool speed_presses_seen;
static uint8_t current_frame, previous_layer = UINT8_MAX, previous_wpm = UINT8_MAX;
static uint32_t frame_tick;
static const uint32_t active_color = 0x00ffe5;

/* Capture before display startup, and retain both slots when events coalesce.
 * All LVGL calls remain on the display queue; event context only changes data. */
K_MUTEX_DEFINE(mascot_mutex);
static struct mascot_input input_state;
static struct { bool connected, reported; uint8_t level; } batteries[2];
static bool caps_word_active;
/* Caps box: host Caps Lock in the accent color, caps-word alone in amber. */
static const uint32_t caps_word_color = 0xffb802;

static int mascot_events(const zmk_event_t *eh) {
    k_mutex_lock(&mascot_mutex, K_FOREVER);
    const struct zmk_keycode_state_changed *key = as_zmk_keycode_state_changed(eh);
    if (key && key->usage_page == HID_USAGE_KEY) {
        /* Modifiers alone do not start running. */
        if (key->state && key->keycode > 3 && key->keycode < 0xe0) {
            input_state.last_press = k_uptime_get();
            input_state.typed = true;
        }
    }
    const struct zmk_peripheral_battery_state_changed *bat =
        as_zmk_peripheral_battery_state_changed(eh);
    if (bat && bat->source < 2) {
        batteries[bat->source].level = MIN(bat->state_of_charge, 100);
        batteries[bat->source].reported = true;
    }
    const struct zmk_caps_word_state_changed *caps_word = as_zmk_caps_word_state_changed(eh);
    if (caps_word) {
        caps_word_active = caps_word->active;
    }
    const struct zmk_split_central_status_changed *conn =
        as_zmk_split_central_status_changed(eh);
    if (conn && conn->slot < 2) {
        batteries[conn->slot].connected = conn->connected;
        if (!conn->connected) {
            batteries[conn->slot].reported = false;
        }
    }
    k_mutex_unlock(&mascot_mutex);
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(mascot_state, mascot_events);
ZMK_SUBSCRIPTION(mascot_state, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(mascot_state, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(mascot_state, zmk_split_central_status_changed);
ZMK_SUBSCRIPTION(mascot_state, zmk_caps_word_state_changed);

#if !IS_ENABLED(CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR)
/* Backlight follows ZMK activity (keys and trackball input); the panel itself
 * is blanked by ZMK_DISPLAY_BLANK_ON_IDLE. */
static int backlight_events(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev) {
        led_set_brightness(DEVICE_DT_GET_ONE(pwm_leds), DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl)),
                           ev->state == ZMK_ACTIVITY_ACTIVE ? CONFIG_PROSPECTOR_FIXED_BRIGHTNESS
                                                            : 0);
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(backlight_state, backlight_events);
ZMK_SUBSCRIPTION(backlight_state, zmk_activity_state_changed);
#endif

static lv_obj_t *text(lv_obj_t *parent, const char *value, const lv_font_t *font,
                      int x, int y, uint32_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, value);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

/* color 0 = off. */
static void set_indicator(int i, uint32_t color) {
    lv_obj_set_style_border_color(lock_boxes[i], lv_color_hex(color ? color : 0x37323c), 0);
    lv_obj_set_style_bg_color(lock_boxes[i], lv_color_hex(color == active_color ? 0x002b27 :
                                                          color ? 0x2b2000 : 0x000000), 0);
    uint32_t glyph = color ? color : 0x77717d;
    lv_obj_set_style_text_color(lock_glyphs[i], lv_color_hex(glyph), 0);
    lv_obj_set_style_line_color(lock_glyphs[i], lv_color_hex(glyph), 0);
    previous_colors[i] = color;
}

static void set_lock(int i, bool active) { set_indicator(i, active ? active_color : 0); }

static void screen_tick(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    struct mascot_input state;
    bool connected[2], reported[2];
    uint8_t level[2];
    k_mutex_lock(&mascot_mutex, K_FOREVER);
    state = input_state;
    bool caps_word = caps_word_active;
    for (int i = 0; i < 2; i++) {
        connected[i] = batteries[i].connected;
        reported[i] = batteries[i].reported;
        level[i] = batteries[i].level;
    }
    k_mutex_unlock(&mascot_mutex);

    uint8_t layer = zmk_keymap_highest_layer_active();
    if (layer != previous_layer) {
        const char *name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(layer));
        if (name && *name) { lv_label_set_text(layer_label, name); }
        else { lv_label_set_text_fmt(layer_label, "L%u", layer); }
        previous_layer = layer;
    }
    uint32_t now = lv_tick_get();
    uint8_t wpm = zmk_wpm_get_state();
    if (wpm != previous_wpm && (int32_t)(now - speed_flash_until) >= 0) {
        lv_label_set_text_fmt(speed_label, "%u WPM", wpm);
        lv_obj_set_style_text_color(speed_label, lv_color_hex(0x98909f), 0);
        previous_wpm = wpm;
    }
    uint8_t next = current_frame;
    if (!mascot_running(&state, k_uptime_get())) { next = 0; }
    else if (current_frame == 0 || now - frame_tick >= mascot_frame_ms(wpm)) {
        next = current_frame % 3 + 1;
        frame_tick = now;
    }
    if (next != current_frame) {
        lv_image_set_src(pet, &mascot_frames[next]);
        current_frame = next;
    }
    uint8_t indicators = zmk_hid_indicators_get_current_profile();
    bool locks[] = { (indicators & BIT(HID_USAGE_LED_CAPS_LOCK - 1)) != 0,
                     (indicators & BIT(HID_USAGE_LED_NUM_LOCK - 1)) != 0 };
    uint32_t colors[] = { locks[0] ? active_color : caps_word ? caps_word_color : 0,
                          locks[1] ? active_color : 0 };
    for (int i = 0; i < 2; i++) {
        if (colors[i] != previous_colors[IND_CAPS + i]) { set_indicator(IND_CAPS + i, colors[i]); }
    }
#if IS_ENABLED(CONFIG_ZMK_VOLTBALL_POINTER_MODE)
    struct voltball_ptr_status ptr;
    voltball_ptr_get_status(&ptr);
    if ((ptr.scroll ? active_color : 0) != previous_colors[IND_SCROLL]) {
        set_lock(IND_SCROLL, ptr.scroll);
    }
    if (speed_presses_seen && ptr.speed_presses != previous_speed_presses) {
        if (ptr.div == 1) { lv_label_set_text_fmt(speed_label, "x%d", (int)ptr.mul); }
        else { lv_label_set_text_fmt(speed_label, "x%d/%d", (int)ptr.mul, (int)ptr.div); }
        lv_obj_set_style_text_color(speed_label, lv_color_hex(active_color), 0);
        speed_flash_until = now + SPEED_FLASH_MS;
        previous_wpm = UINT8_MAX;
    }
    previous_speed_presses = ptr.speed_presses;
    speed_presses_seen = true;
#endif
    static uint16_t previous_battery[2] = { UINT16_MAX, UINT16_MAX };
    for (int i = 0; i < 2; i++) {
        uint16_t value = !connected[i] ? 256 : !reported[i] ? 257 : level[i];
        if (value == previous_battery[i]) { continue; }
        previous_battery[i] = value;
        if (value == 256) { lv_label_set_text_fmt(battery_labels[i], "%c OFF", i ? 'R' : 'L'); }
        else if (value == 257) { lv_label_set_text_fmt(battery_labels[i], "%c N/A", i ? 'R' : 'L'); }
        else { lv_label_set_text_fmt(battery_labels[i], "%c %u%%", i ? 'R' : 'L', value); }
        lv_bar_set_value(battery_bars[i], value <= 100 ? value : 0, LV_ANIM_OFF);
        lv_obj_set_style_text_color(battery_labels[i],
                                   lv_color_hex(value > 100 ? 0x98909f : 0xddd7df), 0);
        lv_obj_set_style_bg_color(battery_bars[i],
                                 lv_color_hex(value > 100 ? 0x98909f :
                                              value < 20 ? 0xff6b6b :
                                              value < 50 ? 0xffb802 : 0x7bd88f),
                                 LV_PART_INDICATOR);
    }
}

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    layer_label = text(screen, "BASE", &lv_font_montserrat_28, 18, 16, 0xf4f2f5);
    lv_obj_set_width(layer_label, 140);
    lv_label_set_long_mode(layer_label, LV_LABEL_LONG_DOT);

    /* Draw Caps directly to avoid missing Unicode glyphs in embedded fonts. */
    static const lv_point_precise_t caps[] = {
        {4,13},{4,9},{0,9},{8,1},{16,9},{12,9},{12,13},{4,13},{4,17},{12,17}
    };
    /* Up/down arrow, retraced as one polyline. */
    static const lv_point_precise_t scroll[] = {
        {4,5},{8,1},{12,5},{8,1},{8,19},{4,15},{8,19},{12,15}
    };
    for (int i = 0; i < IND_COUNT; i++) {
        if (i == IND_SCROLL && !IS_ENABLED(CONFIG_ZMK_VOLTBALL_POINTER_MODE)) { continue; }
        lock_boxes[i] = lv_obj_create(screen);
        lv_obj_remove_style_all(lock_boxes[i]);
        lv_obj_set_pos(lock_boxes[i], 166 + i * 35, 20);
        lv_obj_set_size(lock_boxes[i], 26, 26);
        lv_obj_set_style_border_width(lock_boxes[i], 1, 0);
        lv_obj_set_style_radius(lock_boxes[i], 4, 0);
        lv_obj_set_style_bg_opa(lock_boxes[i], LV_OPA_COVER, 0);
        if (i == IND_NUM) {
            lock_glyphs[i] = text(lock_boxes[i], "1", &lv_font_montserrat_16, 0, 0, 0x77717d);
            lv_obj_center(lock_glyphs[i]);
        } else {
            lock_glyphs[i] = lv_line_create(lock_boxes[i]);
            lv_line_set_points(lock_glyphs[i], i == IND_CAPS ? caps : scroll,
                               i == IND_CAPS ? ARRAY_SIZE(caps) : ARRAY_SIZE(scroll));
            lv_obj_set_pos(lock_glyphs[i], i == IND_CAPS ? 4 : 5, 3);
            lv_obj_set_style_line_width(lock_glyphs[i], 2, 0);
        }
        set_lock(i, false);
    }

    pet = lv_image_create(screen);
    lv_image_set_src(pet, &mascot_frames[0]);
    lv_obj_set_pos(pet, 47, 66);
    zmk_widget_modifier_indicator_init(&modifier_widget, screen);
    lv_obj_t *mods = zmk_widget_modifier_indicator_obj(&modifier_widget);
    lv_obj_set_pos(mods, 232, 59);
    lv_obj_set_style_pad_row(mods, 4, 0);

    speed_label = text(screen, "0 WPM", &lv_font_montserrat_12, 108, 214, 0x98909f);
    lv_obj_set_width(speed_label, 64);
    lv_obj_set_style_text_align(speed_label, LV_TEXT_ALIGN_CENTER, 0);
    for (int i = 0; i < 2; i++) {
        int x = i ? 198 : 18;
        battery_labels[i] = text(screen, i ? "R N/A" : "L N/A", &lv_font_montserrat_12, x, 210, 0x98909f);
        lv_obj_set_width(battery_labels[i], 64);
        lv_obj_set_style_text_align(battery_labels[i], i ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);
        battery_bars[i] = lv_bar_create(screen);
        lv_obj_remove_style_all(battery_bars[i]);
        lv_obj_set_pos(battery_bars[i], x, 228);
        lv_obj_set_size(battery_bars[i], 64, 3);
        lv_obj_set_style_bg_color(battery_bars[i], lv_color_hex(0x302a32), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(battery_bars[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(battery_bars[i], lv_color_hex(0x98909f), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(battery_bars[i], LV_OPA_COVER, LV_PART_INDICATOR);
    }
    screen_tick(NULL);
    lv_timer_create(screen_tick, 25, NULL);
    return screen;
}
