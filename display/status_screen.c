/* Landscape mascot screen for the pinned Prospector module, LVGL 9.
 * Installed only into this repository's dongle build, where pointer mode is on
 * and the ambient light sensor is off. */
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
#include <voltball/pointer_mode.h>
#include "modifier_indicator.h"
#include "mascot_logic.h"

/* Palette. An indicator color of 0 means off. */
#define COLOR_ACCENT 0x00ffe5
#define COLOR_ACCENT_DIM 0x002b27
#define COLOR_AMBER 0xffb802 /* caps-word, battery 20-49% */
#define COLOR_AMBER_DIM 0x2b2000
#define COLOR_GREEN 0x7bd88f
#define COLOR_RED 0xff6b6b
#define COLOR_TEXT 0xf4f2f5
#define COLOR_LABEL 0xddd7df
#define COLOR_MUTED 0x98909f
#define COLOR_GLYPH_OFF 0x77717d
#define COLOR_BOX_OFF 0x37323c
#define COLOR_BAR_BG 0x302a32

extern const lv_image_dsc_t mascot_frames[MASCOT_FRAME_COUNT];
static struct zmk_widget_modifier_indicator modifier_widget;
static lv_obj_t *pet, *layer_label, *speed_label;
/* Indicator boxes left to right after the layer name: Scroll, Caps, NumLock. */
enum { IND_SCROLL, IND_CAPS, IND_NUM, IND_COUNT };
static lv_obj_t *lock_boxes[IND_COUNT], *lock_glyphs[IND_COUNT];
static lv_obj_t *battery_labels[2], *battery_bars[2];
static uint32_t previous_colors[IND_COUNT] = {[0 ... IND_COUNT - 1] = UINT32_MAX}; /* unset */
/* A speed key press shows the new ratio in the WPM slot for this long. */
#define SPEED_FLASH_MS 1000
static uint32_t speed_flash_until, previous_speed_presses;
static uint8_t current_frame, previous_layer = UINT8_MAX, previous_wpm = UINT8_MAX;
static enum mascot_gait current_gait;
static uint32_t frame_tick;
/* The 160x132 mascot slot; narrower frames are centered in it. */
#define MASCOT_X 47
#define MASCOT_Y 66
#define MASCOT_SLOT_WIDTH 160

/* Written from event context, read by the display timer; copied under the
 * mutex so the timer sees a consistent snapshot. All LVGL calls stay on the
 * display queue. */
static K_MUTEX_DEFINE(shared_mutex);
static struct shared {
    struct mascot_input input;
    bool caps_word;
    struct { bool connected, reported; uint8_t level; } battery[2];
} shared;

static int mascot_events(const zmk_event_t *eh) {
    k_mutex_lock(&shared_mutex, K_FOREVER);
    const struct zmk_keycode_state_changed *key = as_zmk_keycode_state_changed(eh);
    if (key && key->usage_page == HID_USAGE_KEY) {
        /* Modifiers alone do not start running. */
        if (key->state && key->keycode > 3 && key->keycode < 0xe0) {
            shared.input.last_press = k_uptime_get();
            shared.input.typed = true;
        }
    }
    const struct zmk_peripheral_battery_state_changed *bat =
        as_zmk_peripheral_battery_state_changed(eh);
    if (bat && bat->source < 2) {
        shared.battery[bat->source].level = MIN(bat->state_of_charge, 100);
        shared.battery[bat->source].reported = true;
    }
    const struct zmk_caps_word_state_changed *caps_word = as_zmk_caps_word_state_changed(eh);
    if (caps_word) {
        shared.caps_word = caps_word->active;
    }
    const struct zmk_split_central_status_changed *conn =
        as_zmk_split_central_status_changed(eh);
    if (conn && conn->slot < 2) {
        shared.battery[conn->slot].connected = conn->connected;
        if (!conn->connected) {
            shared.battery[conn->slot].reported = false;
        }
    }
    k_mutex_unlock(&shared_mutex);
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(mascot_state, mascot_events);
ZMK_SUBSCRIPTION(mascot_state, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(mascot_state, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(mascot_state, zmk_split_central_status_changed);
ZMK_SUBSCRIPTION(mascot_state, zmk_caps_word_state_changed);

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

static lv_obj_t *text(lv_obj_t *parent, const char *value, const lv_font_t *font,
                      int x, int y, uint32_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, value);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

/* Restyles indicator i when its color changes; 0 = off. */
static void set_indicator(int i, uint32_t color) {
    if (color == previous_colors[i]) {
        return;
    }
    previous_colors[i] = color;
    uint32_t border = color ? color : COLOR_BOX_OFF;
    uint32_t fill = color == COLOR_ACCENT ? COLOR_ACCENT_DIM : color ? COLOR_AMBER_DIM : 0x000000;
    uint32_t glyph = color ? color : COLOR_GLYPH_OFF;
    lv_obj_set_style_border_color(lock_boxes[i], lv_color_hex(border), 0);
    lv_obj_set_style_bg_color(lock_boxes[i], lv_color_hex(fill), 0);
    lv_obj_set_style_text_color(lock_glyphs[i], lv_color_hex(glyph), 0);
    lv_obj_set_style_line_color(lock_glyphs[i], lv_color_hex(glyph), 0);
}

static void screen_tick(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    k_mutex_lock(&shared_mutex, K_FOREVER);
    struct shared s = shared;
    k_mutex_unlock(&shared_mutex);

    uint8_t layer = zmk_keymap_highest_layer_active();
    if (layer != previous_layer) {
        lv_label_set_text(layer_label, zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(layer)));
        previous_layer = layer;
    }
    uint32_t now = lv_tick_get();
    uint8_t wpm = zmk_wpm_get_state();
    if (wpm != previous_wpm && (int32_t)(now - speed_flash_until) >= 0) {
        lv_label_set_text_fmt(speed_label, "%u WPM", wpm);
        lv_obj_set_style_text_color(speed_label, lv_color_hex(COLOR_MUTED), 0);
        previous_wpm = wpm;
    }
    /* Walk while typing slowly, ride the scooter when fast; a gait change
     * shows its first frame at once. */
    enum mascot_gait gait =
        mascot_gait_next(current_gait, mascot_running(&s.input, k_uptime_get()), wpm);
    uint8_t next = current_frame;
    if (gait != current_gait ||
        (gait != MASCOT_IDLE && now - frame_tick >= mascot_frame_ms(gait, wpm))) {
        next = mascot_next_frame(gait, current_frame);
        frame_tick = now;
        current_gait = gait;
    }
    if (next != current_frame) {
        lv_image_set_src(pet, &mascot_frames[next]);
        lv_obj_set_x(pet, MASCOT_X + (MASCOT_SLOT_WIDTH - mascot_frames[next].header.w) / 2);
        current_frame = next;
    }

    uint8_t indicators = zmk_hid_indicators_get_current_profile();
    bool caps_lock = indicators & BIT(HID_USAGE_LED_CAPS_LOCK - 1);
    bool num_lock = indicators & BIT(HID_USAGE_LED_NUM_LOCK - 1);
    struct voltball_ptr_status ptr;
    voltball_ptr_get_status(&ptr);
    /* Caps box: host Caps Lock in the accent color, caps-word alone in amber. */
    uint32_t colors[IND_COUNT] = {
        [IND_SCROLL] = ptr.scroll ? COLOR_ACCENT : 0,
        [IND_CAPS] = caps_lock ? COLOR_ACCENT : s.caps_word ? COLOR_AMBER : 0,
        [IND_NUM] = num_lock ? COLOR_ACCENT : 0,
    };
    for (int i = 0; i < IND_COUNT; i++) {
        set_indicator(i, colors[i]);
    }
    if (ptr.speed_presses != previous_speed_presses) {
        if (ptr.div == 1) { lv_label_set_text_fmt(speed_label, "x%d", (int)ptr.mul); }
        else { lv_label_set_text_fmt(speed_label, "x%d/%d", (int)ptr.mul, (int)ptr.div); }
        lv_obj_set_style_text_color(speed_label, lv_color_hex(COLOR_ACCENT), 0);
        speed_flash_until = now + SPEED_FLASH_MS;
        previous_wpm = UINT8_MAX;
        previous_speed_presses = ptr.speed_presses;
    }

    /* Battery slot value: a level 0-100, or one of these states. */
    enum { BAT_OFF = 256, BAT_NA };
    static uint16_t previous_battery[2] = { UINT16_MAX, UINT16_MAX };
    for (int i = 0; i < 2; i++) {
        uint16_t value = !s.battery[i].connected ? BAT_OFF
                         : !s.battery[i].reported ? BAT_NA : s.battery[i].level;
        if (value == previous_battery[i]) { continue; }
        previous_battery[i] = value;
        char side = i ? 'R' : 'L';
        if (value == BAT_OFF) { lv_label_set_text_fmt(battery_labels[i], "%c OFF", side); }
        else if (value == BAT_NA) { lv_label_set_text_fmt(battery_labels[i], "%c N/A", side); }
        else { lv_label_set_text_fmt(battery_labels[i], "%c %u%%", side, value); }
        bool known = value <= 100;
        lv_bar_set_value(battery_bars[i], known ? value : 0, LV_ANIM_OFF);
        lv_obj_set_style_text_color(battery_labels[i],
                                   lv_color_hex(known ? COLOR_LABEL : COLOR_MUTED), 0);
        lv_obj_set_style_bg_color(battery_bars[i],
                                 lv_color_hex(!known ? COLOR_MUTED :
                                              value < 20 ? COLOR_RED :
                                              value < 50 ? COLOR_AMBER : COLOR_GREEN),
                                 LV_PART_INDICATOR);
    }
}

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    layer_label = text(screen, "BASE", &lv_font_montserrat_28, 18, 16, COLOR_TEXT);
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
        lock_boxes[i] = lv_obj_create(screen);
        lv_obj_remove_style_all(lock_boxes[i]);
        lv_obj_set_pos(lock_boxes[i], 166 + i * 35, 20);
        lv_obj_set_size(lock_boxes[i], 26, 26);
        lv_obj_set_style_border_width(lock_boxes[i], 1, 0);
        lv_obj_set_style_radius(lock_boxes[i], 4, 0);
        lv_obj_set_style_bg_opa(lock_boxes[i], LV_OPA_COVER, 0);
        if (i == IND_NUM) {
            lock_glyphs[i] = text(lock_boxes[i], "1", &lv_font_montserrat_16, 0, 0, COLOR_GLYPH_OFF);
            lv_obj_center(lock_glyphs[i]);
        } else {
            lock_glyphs[i] = lv_line_create(lock_boxes[i]);
            lv_line_set_points(lock_glyphs[i], i == IND_CAPS ? caps : scroll,
                               i == IND_CAPS ? ARRAY_SIZE(caps) : ARRAY_SIZE(scroll));
            lv_obj_set_pos(lock_glyphs[i], i == IND_CAPS ? 4 : 5, 3);
            lv_obj_set_style_line_width(lock_glyphs[i], 2, 0);
        }
        set_indicator(i, 0);
    }

    pet = lv_image_create(screen);
    lv_image_set_src(pet, &mascot_frames[MASCOT_IDLE_FRAME]);
    lv_obj_set_pos(pet, MASCOT_X + (MASCOT_SLOT_WIDTH - mascot_frames[MASCOT_IDLE_FRAME].header.w) / 2,
                   MASCOT_Y);
    zmk_widget_modifier_indicator_init(&modifier_widget, screen);
    lv_obj_t *mods = zmk_widget_modifier_indicator_obj(&modifier_widget);
    lv_obj_set_pos(mods, 232, 59);
    lv_obj_set_style_pad_row(mods, 4, 0);

    speed_label = text(screen, "0 WPM", &lv_font_montserrat_12, 108, 214, COLOR_MUTED);
    lv_obj_set_width(speed_label, 64);
    lv_obj_set_style_text_align(speed_label, LV_TEXT_ALIGN_CENTER, 0);
    for (int i = 0; i < 2; i++) {
        int x = i ? 198 : 18;
        battery_labels[i] = text(screen, i ? "R N/A" : "L N/A", &lv_font_montserrat_12, x, 210, COLOR_MUTED);
        lv_obj_set_width(battery_labels[i], 64);
        lv_obj_set_style_text_align(battery_labels[i], i ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);
        battery_bars[i] = lv_bar_create(screen);
        lv_obj_remove_style_all(battery_bars[i]);
        lv_obj_set_pos(battery_bars[i], x, 228);
        lv_obj_set_size(battery_bars[i], 64, 3);
        lv_obj_set_style_bg_color(battery_bars[i], lv_color_hex(COLOR_BAR_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(battery_bars[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(battery_bars[i], lv_color_hex(COLOR_MUTED), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(battery_bars[i], LV_OPA_COVER, LV_PART_INDICATOR);
    }
    screen_tick(NULL);
    lv_timer_create(screen_tick, 25, NULL);
    return screen;
}
