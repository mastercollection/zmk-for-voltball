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

static int typing_calls;
static void count_typing(const struct device *behavior) { (void)behavior; typing_calls++; }

/* Voltball MOUSE layer shape: thumbs 35-39 are buttons, the rest &trans. */
static void mouse_layer(void) {
    for (uint32_t i = 0; i < 42; i++) {
        mouse_bindings[i].behavior_dev = i >= 35 && i <= 39 ? "mkp" : "trans";
    }
    mouse_bindings[8].behavior_dev = "ptr";
}

static void activate(void) {
    memset(&data, 0, sizeof(data));
    memset(active_layers, 0, sizeof(active_layers));
    memset(locked_layers, 0, sizeof(locked_layers));
    mouse_layer();
    mock_device.config = &config;
    mock_device.data = &data;
    assert(threshold_init(&mock_device) == 0);
    fake_now += 1000;
    move(100);
    assert(active_layers[5]);
}

int main(void) {
    (void)threshold_api;
    /* Before any pointer event the mouse layer is unknown: nothing exits. */
    memset(&data, 0, sizeof(data));
    mock_device.config = &config;
    mock_device.data = &data;
    assert(threshold_init(&mock_device) == 0);
    mouse_layer();
    assert(!is_exit_key(data.mouse_layer, 0));

    activate();
    fake_now += 3600000;
    move(0);
    assert(active_layers[5]);

    /* Ctrl/Shift/Alt/GUI and sticky modifiers on &trans keys never exit. */
    for (uint32_t key = 0xe0; key <= 0xe7; key++) {
        assert(!filter(&device_kp, 32, (7u << 16) | key, true));
        assert(!filter(&device_kp, 32, (7u << 16) | key, false));
        assert(!filter(&device_sk, 32, (7u << 16) | key, true));
        assert(!filter(&device_sk, 32, (7u << 16) | key, false));
        assert(active_layers[5]);
    }
    /* Keys placed on the mouse layer (buttons, &ptr) keep auto mouse. */
    for (uint32_t pos = 35; pos <= 39; pos++) {
        assert(!filter(&device_click, pos, 1, true));
        assert(!filter(&device_click, pos, 1, false));
        assert(active_layers[5]);
    }
    assert(!filter(&device_kp, 8, 0, true));
    assert(active_layers[5]);
    /* Transparent lookup and hold-tap wrappers must not themselves exit;
     * a hold resolving to Shift keeps auto mouse for Shift+click. */
    assert(!filter(&device_trans, 13, 0, true));
    assert(!filter(&device_mt, 13, 0xe1, true));
    assert(!filter(&device_kp, 13, (7u << 16) | 0xe1, true));
    assert(active_layers[5]);
    assert(!filter(&device_kp, 13, (7u << 16) | 0xe1, false));

    /* A &trans key exits and is delivered: never consumed, on either edge. */
    assert(!filter(&device_kp, 13, 9, true));
    assert(!active_layers[5]);
    assert(!filter(&device_kp, 13, 9, false));

    /* Any &trans position exits, both halves, any resolved action. */
    const struct device *actions[] = {&device_kp, &device_scroll, &device_layer, &device_none};
    for (size_t a = 0; a < ARRAY_SIZE(actions); a++) {
        for (uint32_t pos = 0; pos < 42; pos++) {
            if (pos == 8 || (pos >= 35 && pos <= 39)) {
                continue;
            }
            activate();
            uint32_t param = a == 0 ? 20 : 0xe1;
            assert(!filter(actions[a], pos, param, true));
            assert(!active_layers[5]);
            assert(!filter(actions[a], pos, param, false));
        }
    }

    /* Cooldown after typing; motion during it is discarded. */
    activate();
    assert(!filter(&device_kp, 1, 20, true));
    assert(!active_layers[5]);
    fake_now += 199;
    move(1000); assert(!active_layers[5]);
    fake_now++;
    move(99); assert(!active_layers[5]);
    move(1); assert(active_layers[5]);

    /* Combos exit when any key is &trans on the mouse layer. */
    mouse_bindings[6].behavior_dev = "mkp";
    mouse_bindings[7].behavior_dev = "mkp";
    assert(!filter(&device_kp, 43, 48, true));
    assert(active_layers[5]);
    assert(!filter(&device_kp, 42, 47, true));
    assert(!active_layers[5]);

    /* Live keymap: a key rebound to a button stops exiting. */
    activate();
    mouse_bindings[13].behavior_dev = "mkp";
    assert(!filter(&device_click, 13, 1, true));
    assert(active_layers[5]);

    /* A manually locked mouse layer is not forced off. */
    activate();
    locked_layers[5] = true;
    assert(!filter(&device_kp, 0, 20, true));
    assert(active_layers[5] && !data.layers[5].active);

    /* A stale internal state after an external layer exit re-arms cleanly. */
    activate();
    active_layers[5] = false;
    assert(!filter(&device_kp, 0, 20, true));
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

    /* Wheel events still record the mouse layer (scroll mode before any pointer motion). */
    memset(&data, 0, sizeof(data));
    assert(threshold_init(&mock_device) == 0);
    assert(threshold_handle_event(&mock_device, &wheel, 5, 0, NULL) == 0);
    assert(data.mouse_layer == 5);

    /* Pointer mode hears every exit-key press, with or without auto mouse. */
    voltball_auto_mouse_set_typing_callback(count_typing);
    activate();
    typing_calls = 0;
    assert(!filter(&device_kp, 0, 20, true) && typing_calls == 1);
    assert(!filter(&device_kp, 0, 20, false) && typing_calls == 1);
    assert(!filter(&device_kp, 0, 20, true) && typing_calls == 2);
    assert(!filter(&device_kp, 32, (7u << 16) | 0xe0, true) && typing_calls == 2);
    assert(!filter(&device_click, 36, 1, true) && typing_calls == 2);
    voltball_auto_mouse_set_typing_callback(NULL);
    puts("PASS: actual driver: &trans exits and types, buttons stay, modifiers, hold-tap, combos, live keymap, cooldown, locks, typing callback");
    return 0;
}
