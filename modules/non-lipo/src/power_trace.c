/* SPDX-License-Identifier: MIT */
#include <string.h>
#include <stddef.h>
#include <errno.h>
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
#define TRACE_VERSION 2
#define BOOT_KEY "voltball_power/boots"
#define BOOT_HISTORY_SIZE 8

/* Fixed-size fields allow rejecting records written by incompatible builds. */
struct shutdown_record {
    int64_t uptime_ms;
    int64_t sample_ms;
    uint32_t version;
    uint32_t reason;
    uint32_t adc_mv;
    uint32_t valid;
    uint32_t boot_id;
    uint32_t reserved;
};

struct boot_entry {
    uint32_t id;
    uint32_t cause;
    int32_t cause_rc;
};

struct boot_history {
    uint32_t version;
    uint32_t count;
    struct boot_entry entries[BOOT_HISTORY_SIZE];
};

static struct shutdown_record battery;
static struct shutdown_record previous;
static struct k_spinlock battery_lock;
static uint32_t boot_cause;
static int boot_cause_rc;
static bool previous_loaded;
static struct boot_history boots = {.version = 1};
static bool boot_registered;
static int boot_save_rc = -EAGAIN;
static int shutdown_load_rc = -ENOENT;
static int boot_load_rc = -ENOENT;
static uint32_t current_boot_id;
/* Settings callbacks and shutdown paths may run on different threads. */
K_MUTEX_DEFINE(history_mutex);

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
    LOG_INF("[power] %s boot_id=%u reason=%u uptime_ms=%lld adc_mv=%u cell_mv_est=%u "
            "sample_valid=%u sample_age_ms=%lld", label, record->boot_id, record->reason,
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
    k_mutex_lock(&history_mutex, K_FOREVER);
    /* Zero explicitly means attribution is unavailable (e.g. boot save failed). */
    record.boot_id = current_boot_id;
    k_mutex_unlock(&history_mutex);
    log_record("shutdown-request", &record);
    int rc = settings_save_one(TRACE_KEY, &record, sizeof(record));
    k_mutex_lock(&history_mutex, K_FOREVER);
    if (rc == 0) {
        previous = record;
        previous_loaded = true;
    }
    LOG_INF("[power] shutdown record save rc=%d", rc);
    k_mutex_unlock(&history_mutex);
}

static int load_record(const char *name, size_t len, settings_read_cb read_cb, void *arg) {
    if (strcmp(name, "boots") == 0) {
        struct boot_history loaded;
        boot_load_rc = -EINVAL;
        if (len != sizeof(loaded)) {
            return boot_load_rc;
        }
        ssize_t count = read_cb(arg, &loaded, sizeof(loaded));
        if (count != sizeof(loaded) || loaded.version != 1 ||
            loaded.count > BOOT_HISTORY_SIZE) {
            return boot_load_rc;
        }
        k_mutex_lock(&history_mutex, K_FOREVER);
        if (!boot_registered) {
            boots = loaded;
        }
        boot_load_rc = 0;
        k_mutex_unlock(&history_mutex);
        return 0;
    }
    if (strcmp(name, "last") != 0) {
        return -ENOENT;
    }
    /* Version 1 ended before boot_id. Preserve records from the previous UF2,
     * with boot_id=0 to avoid attributing old evidence to the latest boot. */
    const size_t legacy_size = offsetof(struct shutdown_record, boot_id);
    struct shutdown_record loaded = {0};
    shutdown_load_rc = -EINVAL;
    if (len != legacy_size && len != sizeof(loaded)) {
        return shutdown_load_rc;
    }
    ssize_t count = read_cb(arg, &loaded, len);
    if (count != len || !((len == legacy_size && loaded.version == 1) ||
                         (len == sizeof(loaded) && loaded.version == TRACE_VERSION))) {
        return shutdown_load_rc;
    }
    k_mutex_lock(&history_mutex, K_FOREVER);
    previous = loaded;
    previous_loaded = true;
    shutdown_load_rc = 0;
    k_mutex_unlock(&history_mutex);
    return 0;
}

static int register_boot(void) {
    k_mutex_lock(&history_mutex, K_FOREVER);
    if (!boot_registered) {
        uint32_t next_id = boots.count ? boots.entries[boots.count - 1].id + 1 : 1;
        /* Reserve zero for unknown attribution, including legacy records. */
        if (next_id == 0) {
            next_id = 1;
        }
        if (boots.count == BOOT_HISTORY_SIZE) {
            memmove(&boots.entries[0], &boots.entries[1],
                    sizeof(boots.entries[0]) * (BOOT_HISTORY_SIZE - 1));
            boots.count--;
        }
        boots.entries[boots.count++] = (struct boot_entry){
            .id = next_id, .cause = boot_cause, .cause_rc = boot_cause_rc,
        };
        boot_registered = true;
        struct boot_history saved = boots;
        /* settings_load already holds the settings mutex. Never acquire it
         * while holding history_mutex, including from shutdown callbacks. */
        k_mutex_unlock(&history_mutex);
        int rc = settings_save_one(BOOT_KEY, &saved, sizeof(saved));
        k_mutex_lock(&history_mutex, K_FOREVER);
        boot_save_rc = rc;
        if (boot_save_rc == 0) {
            current_boot_id = next_id;
        }
        LOG_INF("[power] boot history save rc=%d boot_id=%u", boot_save_rc, current_boot_id);
    }
    k_mutex_unlock(&history_mutex);
    /* A diagnostic storage failure must not fail other ZMK settings commits. */
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(voltball_power, "voltball_power", NULL, load_record,
                               register_boot, NULL);

static void heartbeat(struct k_work *work) {
    k_mutex_lock(&history_mutex, K_FOREVER);
    LOG_INF("[power] boot cause=0x%08x rc=%d pin=%u software=%u watchdog=%u "
            "lockup=%u low_power_wake=%u por=%u brownout=%u usb=%u previous=%u", boot_cause, boot_cause_rc,
            !!(boot_cause & RESET_PIN), !!(boot_cause & RESET_SOFTWARE),
            !!(boot_cause & RESET_WATCHDOG), !!(boot_cause & RESET_CPU_LOCKUP),
            !!(boot_cause & RESET_LOW_POWER_WAKE), !!(boot_cause & RESET_POR),
            !!(boot_cause & RESET_BROWNOUT), zmk_usb_is_powered(), previous_loaded);
    struct shutdown_record record = snapshot();
    record.boot_id = current_boot_id;
    log_record("alive", &record);
    LOG_INF("[power] history boot_load_rc=%d boot_save_rc=%d shutdown_load_rc=%d",
            boot_load_rc, boot_save_rc, shutdown_load_rc);
    for (uint32_t i = 0; i < boots.count; i++) {
        LOG_INF("[power] boot-history boot_id=%u cause=0x%08x rc=%d",
                boots.entries[i].id, boots.entries[i].cause, boots.entries[i].cause_rc);
    }
    if (previous_loaded) {
        log_record("last-saved-shutdown-request", &previous);
    }
    k_mutex_unlock(&history_mutex);
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
