/* Included after the actual driver source by the host regression harness. */
static void move(int32_t delta) {
    struct input_event event = {INPUT_EV_REL, INPUT_REL_X, delta};
    assert(threshold_handle_event(&mock_device, &event, 5, 0, NULL) == 0);
}

static bool filter(const struct device *behavior, uint32_t position, uint32_t key, bool press) {
    struct zmk_behavior_binding binding = {NULL, key, 0};
    struct zmk_behavior_binding_event event = {position};
    return voltball_auto_mouse_filter(behavior, &binding, event, press);
}

static void activate(void) {
    memset(&data, 0, sizeof(data));
    memset(active_layers, 0, sizeof(active_layers));
    memset(locked_layers, 0, sizeof(locked_layers));
    mock_device.config = &config;
    mock_device.data = &data;
    assert(threshold_init(&mock_device) == 0);
    fake_now += 1000;
    move(100);
    assert(active_layers[5]);
}

int main(void) {
    (void)threshold_api;
    activate();
    fake_now += 3600000;
    move(0);
    assert(active_layers[5]);

    /* Left and right Ctrl/Shift/Alt/GUI and sticky modifiers pass both edges. */
    for (uint32_t key = 0xe0; key <= 0xe7; key++) {
        assert(!filter(&device_kp, 32, (7u << 16) | key, true));
        assert(!filter(&device_kp, 32, (7u << 16) | key, false));
        assert(!filter(&device_sk, 32, (7u << 16) | key, true));
        assert(!filter(&device_sk, 32, (7u << 16) | key, false));
        assert(active_layers[5]);
    }
    /* Physical right half does not cancel or swallow. */
    for (uint32_t pos = 0; pos < 42; pos++) {
        if (!is_typing_position(pos)) {
            assert(!filter(&device_kp, pos, 20, true));
            assert(!filter(&device_kp, pos, 20, false));
            assert(active_layers[5]);
        }
    }
    /* Transparent lookup and hold-tap wrappers must not themselves cancel. */
    assert(!filter(&device_trans, 13, 0, true));
    assert(!filter(&device_mt, 13, 0xe1, true));
    assert(!filter(&device_kp, 13, (7u << 16) | 0xe1, true));
    assert(active_layers[5]);
    assert(!filter(&device_mt, 13, 0xe1, false));
    assert(!filter(&device_kp, 13, (7u << 16) | 0xe1, false));

    /* F tap is swallowed; release of the parent must still clean up hold-tap. */
    assert(!filter(&device_mt, 13, 0xe1, true));
    assert(filter(&device_kp, 13, 9, true));
    assert(!active_layers[5] && data.swallowed_count == 1);
    assert(!filter(&device_mt, 13, 0xe1, false));
    assert(filter(&device_kp, 13, 9, false));
    assert(data.swallowed_count == 0);
    assert(!filter(&device_kp, 13, 9, true));
    assert(!filter(&device_kp, 13, 9, false));

    /* Every physical left position can exit; unrelated HID modifiers on a
     * scroll/mouse binding must not be mistaken for a modifier key. */
    const struct device *actions[] = {&device_kp, &device_scroll, &device_click,
                                      &device_layer, &device_none};
    for (size_t a = 0; a < ARRAY_SIZE(actions); a++) {
        for (size_t p = 0; p < ARRAY_SIZE(typing_positions); p++) {
            activate();
            uint32_t pos = typing_positions[p];
            uint32_t param = a == 0 ? 20 : 0xe1;
            assert(filter(actions[a], pos, param, true));
            assert(!active_layers[5]);
            assert(filter(actions[a], pos, param, false));
            assert(!filter(actions[a], pos, param, true));
            assert(!filter(actions[a], pos, param, false));
        }
    }

    /* Keep a consumed press/release pair suppressed even after long holds.
     * No reactivation while it is held; cooldown restarts on release. */
    activate();
    assert(filter(&device_scroll, 1, 1, true));
    fake_now += 5000;
    move(1000);
    assert(!active_layers[5]);
    assert(filter(&device_scroll, 1, 1, true));
    assert(data.swallowed_count == 1);
    assert(filter(&device_scroll, 1, 1, false));
    fake_now += 199;
    move(1000); assert(!active_layers[5]);
    fake_now++;
    move(99); assert(!active_layers[5]);
    move(1); assert(active_layers[5]);

    /* Previously delivered presses still get their release after activation. */
    assert(!filter(&device_click, 35, 1, false));
    assert(active_layers[5]);
    /* Right-only combos pass; left-involving combos consume both edges. */
    assert(!filter(&device_kp, 43, 48, true));
    assert(!filter(&device_kp, 43, 48, false));
    assert(filter(&device_kp, 42, 47, true));
    assert(filter(&device_kp, 42, 47, false));
    assert(!active_layers[5]);

    activate();
    locked_layers[5] = true;
    assert(filter(&device_kp, 0, 20, true));
    assert(active_layers[5] && !data.layers[5].active);
    assert(filter(&device_kp, 0, 20, false));

    /* A stale internal state must not eat typing after an external layer exit. */
    activate();
    active_layers[5] = false;
    assert(!filter(&device_kp, 0, 20, true));
    assert(!filter(&device_kp, 0, 20, false));
    fake_now += 200;
    move(100); assert(active_layers[5]);
    active_layers[5] = false;
    move(1); assert(!active_layers[5]);
    move(99); assert(active_layers[5]);
    struct input_event wheel = {INPUT_EV_REL, 8, 100};
    assert(threshold_handle_event(&mock_device, &wheel, 5, 0, NULL) == 0);
    assert(active_layers[5]);
    assert(threshold_handle_event(&mock_device, &wheel, 5, 500, NULL) == -EINVAL);
    assert(threshold_handle_event(&mock_device, &wheel, 256, 0, NULL) == -EINVAL);
    puts("PASS: actual driver: swallowed press/release, modifiers, hold-tap, all left actions, right keys, combos, cooldown, locks");
    return 0;
}
