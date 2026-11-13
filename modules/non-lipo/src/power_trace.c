/* SPDX-License-Identifier: MIT */
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/conn.h>
#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/usb.h>
#include "power_trace.h"

LOG_MODULE_REGISTER(voltball_power, LOG_LEVEL_INF);

#define TRACE_KEY "voltball_power/last"
#define TRACE_VERSION 1

/* Fixed-size fields allow rejecting records written by incompatible builds. */
struct shutdown_record {
    int64_t uptime_ms;
    int64_t sample_ms;
    uint32_t version;
    uint32_t reason;
    uint32_t adc_mv;
    uint32_t valid;
};

static struct shutdown_record battery;
static struct shutdown_record previous;
static struct k_spinlock battery_lock;
static uint32_t boot_cause;
static int boot_cause_rc;
static bool previous_loaded;
static bool history_consumed;

void voltball_trace_voltage(uint16_t adc_mv) {
    k_spinlock_key_t key = k_spin_lock(&battery_lock);
    battery.adc_mv = adc_mv;
    battery.sample_ms = k_uptime_get();
    battery.valid = 1;
    k_spin_unlock(&battery_lock, key);
}

static struct shutdown_record snapshot(void) {
    k_spinlock_key_t key = k_spin_lock(&battery_lock);
    struct shutdown_record record = battery;
    k_spin_unlock(&battery_lock, key);
    record.uptime_ms = k_uptime_get();
    record.version = TRACE_VERSION;
    return record;
}

static void log_record(const char *label, const struct shutdown_record *record) {
    LOG_INF("[power] %s reason=%u uptime_ms=%lld adc_mv=%u cell_mv_est=%u "
            "sample_valid=%u sample_age_ms=%lld", label, record->reason,
            record->uptime_ms, record->adc_mv, record->adc_mv * 2,
            record->valid, record->valid ? record->uptime_ms - record->sample_ms : -1LL);
}

void voltball_trace_shutdown(enum voltball_shutdown_reason reason, bool refresh) {
    if (refresh) {
        const struct device *sensor = DEVICE_DT_GET(DT_CHOSEN(zmk_battery));
        int rc = sensor_sample_fetch_chan(sensor, SENSOR_CHAN_GAUGE_VOLTAGE);
        if (rc < 0) {
            LOG_WRN("[power] pre-shutdown sample failed rc=%d; using last sample", rc);
        }
    }
    struct shutdown_record record = snapshot();
    record.reason = reason;
    log_record("shutdown-request", &record);
    int rc = settings_save_one(TRACE_KEY, &record, sizeof(record));
    LOG_INF("[power] shutdown record save rc=%d", rc);
}

static int load_record(const char *name, size_t len, settings_read_cb read_cb, void *arg) {
    if (strcmp(name, "last") != 0) {
        return -ENOENT;
    }
    if (len != sizeof(previous)) {
        return -EINVAL;
    }
    ssize_t count = read_cb(arg, &previous, sizeof(previous));
    if (count != sizeof(previous) || previous.version != TRACE_VERSION) {
        return -EINVAL;
    }
    previous_loaded = true;
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(voltball_power, "voltball_power", NULL, load_record, NULL, NULL);

static void heartbeat(struct k_work *work) {
    LOG_INF("[power] boot cause=0x%08x rc=%d pin=%u software=%u watchdog=%u "
            "lockup=%u low_power_wake=%u por=%u brownout=%u usb=%u previous=%u", boot_cause, boot_cause_rc,
            !!(boot_cause & RESET_PIN), !!(boot_cause & RESET_SOFTWARE),
            !!(boot_cause & RESET_WATCHDOG), !!(boot_cause & RESET_CPU_LOCKUP),
            !!(boot_cause & RESET_LOW_POWER_WAKE), !!(boot_cause & RESET_POR),
            !!(boot_cause & RESET_BROWNOUT), zmk_usb_is_powered(), previous_loaded);
    struct shutdown_record record = snapshot();
    log_record("alive", &record);
    if (previous_loaded) {
        log_record("previous-shutdown-request", &previous);
        /* Keep a RAM copy for later USB attachment, consume the flash record so
         * an unexpected reset on this boot cannot report an older shutdown. */
        if (!history_consumed) {
            int rc = settings_delete(TRACE_KEY);
            history_consumed = rc == 0;
            LOG_INF("[power] previous record consume rc=%d", rc);
        }
    }
    k_work_reschedule(k_work_delayable_from_work(work), K_SECONDS(30));
}
K_WORK_DELAYABLE_DEFINE(heartbeat_work, heartbeat);

static void connected(struct bt_conn *conn, uint8_t err) {
    LOG_INF("[power] connected err=0x%02x uptime_ms=%lld", err, k_uptime_get());
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    LOG_INF("[power] disconnected reason=0x%02x uptime_ms=%lld", reason, k_uptime_get());
}
BT_CONN_CB_DEFINE(power_trace_connections) = {
    .connected = connected,
    .disconnected = disconnected,
};

static int activity_listener(const zmk_event_t *event) {
    if (as_zmk_activity_state_changed(event)->state == ZMK_ACTIVITY_SLEEP) {
        voltball_trace_shutdown(VOLTBALL_SHUTDOWN_IDLE, true);
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(power_trace, activity_listener);
ZMK_SUBSCRIPTION(power_trace, zmk_activity_state_changed);

static int power_trace_init(void) {
    boot_cause_rc = hwinfo_get_reset_cause(&boot_cause);
    LOG_INF("[power] boot cause=0x%08x rc=%d", boot_cause, boot_cause_rc);
    if (boot_cause_rc == 0) {
        int rc = hwinfo_clear_reset_cause();
        LOG_INF("[power] clear reset cause rc=%d", rc);
    }
    /* ZMK loads settings in main; the delayed work reports them afterwards. */
    k_work_schedule(&heartbeat_work, K_SECONDS(5));
    return 0;
}
SYS_INIT(power_trace_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
