/* SPDX-License-Identifier: MIT */
/*
 * Diagnostic-only trace on the dongle (central) of how a half comes back after
 * deep sleep. Waking from deep sleep is a full reboot of the half, so the
 * dongle can only see it from its first split advertisement onward:
 *
 *   adv seen -> connected -> encrypted -> conn params -> first key/ball input
 *
 * Every line starts with "[wake]" and carries the milliseconds since that
 * half's advertisement was seen, so the log can be filtered for it.
 *
 * A slot is created when a half's advertisement is seen (or when it connects)
 * and freed when it disconnects. Slots are shared between the BT RX thread and
 * the system work queue, so they are touched only under the spinlock.
 */
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/split/bluetooth/uuid.h>

LOG_MODULE_REGISTER(wake_trace, LOG_LEVEL_INF);

#define TRACE_SLOTS 4

struct trace_slot {
    bool used;
    bool connected;
    bool waiting_input;
    bt_addr_le_t addr;
    int64_t t_adv; /* 0 until the advertisement is seen */
};

static struct trace_slot slots[TRACE_SLOTS];
static struct k_spinlock lock;
static const uint8_t split_uuid[16] = {ZMK_SPLIT_BT_SERVICE_UUID};

/* Finds the slot for addr, creating one if needed; NULL when all are in use.
 * Call with the lock held. */
static struct trace_slot *slot_for(const bt_addr_le_t *addr) {
    struct trace_slot *free_slot = NULL;
    for (int i = 0; i < TRACE_SLOTS; i++) {
        if (slots[i].used && bt_addr_le_eq(&slots[i].addr, addr)) {
            return &slots[i];
        }
        if (!slots[i].used && !free_slot) {
            free_slot = &slots[i];
        }
    }
    if (free_slot) {
        *free_slot = (struct trace_slot){.used = true};
        bt_addr_le_copy(&free_slot->addr, addr);
    }
    return free_slot;
}

static int ms_since_adv(const struct trace_slot *s) {
    return s && s->t_adv ? (int)(k_uptime_get() - s->t_adv) : -1;
}

/* Milliseconds since this connection's advertisement was seen, -1 if unknown. */
static int since_adv(struct bt_conn *conn) {
    k_spinlock_key_t key = k_spin_lock(&lock);
    int ms = ms_since_adv(slot_for(bt_conn_get_dst(conn)));
    k_spin_unlock(&lock, key);
    return ms;
}

/* "[wake] <peer address> <fmt>" */
#define WAKE_LOG(conn, fmt, ...)                                                               \
    do {                                                                                       \
        char _addr[BT_ADDR_LE_STR_LEN];                                                        \
        bt_addr_le_to_str(bt_conn_get_dst(conn), _addr, sizeof(_addr));                        \
        LOG_INF("[wake] %s " fmt, _addr, ##__VA_ARGS__);                                       \
    } while (0)

static bool has_split_uuid(struct bt_data *data, void *found) {
    if (data->type == BT_DATA_UUID128_ALL || data->type == BT_DATA_UUID128_SOME) {
        for (int off = 0; off + 16 <= data->data_len; off += 16) {
            if (memcmp(data->data + off, split_uuid, 16) == 0) {
                *(bool *)found = true;
                return false;
            }
        }
    }
    return true;
}

static void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf) {
    /* A bonded half sends directed advertising (no AD data) at the dongle,
     * same as ZMK's split central accepts; an unbonded one sends the split UUID. */
    bool directed = info->adv_type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND;
    bool split = directed;
    if (!split && info->adv_type == BT_GAP_ADV_TYPE_ADV_IND) {
        bt_data_parse(buf, has_split_uuid, &split);
    }
    if (!split) {
        return;
    }

    k_spinlock_key_t key = k_spin_lock(&lock);
    struct trace_slot *s = slot_for(info->addr);
    bool first = s && !s->connected && !s->t_adv;
    if (first) {
        s->t_adv = k_uptime_get();
    }
    k_spin_unlock(&lock, key);

    if (first) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(info->addr, addr, sizeof(addr));
        LOG_INF("[wake] %s adv seen (t=0, %s, rssi %d)", addr,
                directed ? "directed" : "split uuid", info->rssi);
    }
}

static struct bt_le_scan_cb scan_cb = {.recv = scan_recv};

/* The dongle is also a peripheral of the host; only central-role links are halves. */
static bool is_split_conn(struct bt_conn *conn) {
    struct bt_conn_info info;
    return bt_conn_get_info(conn, &info) == 0 && info.role == BT_CONN_ROLE_CENTRAL;
}

static void on_connected(struct bt_conn *conn, uint8_t err) {
    if (!is_split_conn(conn)) {
        return;
    }
    if (err) {
        WAKE_LOG(conn, "connect failed (err 0x%02x)", err);
        return;
    }
    k_spinlock_key_t key = k_spin_lock(&lock);
    struct trace_slot *s = slot_for(bt_conn_get_dst(conn));
    if (s) {
        s->connected = true;
        s->waiting_input = true;
    }
    k_spin_unlock(&lock, key);
    WAKE_LOG(conn, "connected (+%d ms)", since_adv(conn));
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason) {
    if (!is_split_conn(conn)) {
        return;
    }
    k_spinlock_key_t key = k_spin_lock(&lock);
    struct trace_slot *s = slot_for(bt_conn_get_dst(conn));
    if (s) {
        s->used = false;
    }
    k_spin_unlock(&lock, key);
    WAKE_LOG(conn, "disconnected (reason 0x%02x)", reason);
}

static void on_security_changed(struct bt_conn *conn, bt_security_t level,
                                enum bt_security_err err) {
    if (is_split_conn(conn)) {
        WAKE_LOG(conn, "security level %d err %d (+%d ms)", level, err, since_adv(conn));
    }
}

static void on_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency,
                             uint16_t timeout) {
    if (is_split_conn(conn)) {
        /* interval is in 1.25 ms units */
        WAKE_LOG(conn, "conn interval %u.%02u ms latency %u (+%d ms)", (interval * 125) / 100,
                 (interval * 125) % 100, latency, since_adv(conn));
    }
}

BT_CONN_CB_DEFINE(wake_trace_conn_cb) = {
    .connected = on_connected,
    .disconnected = on_disconnected,
    .security_changed = on_security_changed,
    .le_param_updated = on_param_updated,
};

/* The split events do not say which address they came from, so the first input
 * after a reconnect is attributed to whichever half is still waiting for one;
 * wake one half at a time to keep this unambiguous. */
static void first_input(const char *what) {
    struct trace_slot hit = {0};
    int ms = -1;

    k_spinlock_key_t key = k_spin_lock(&lock);
    for (int i = 0; i < TRACE_SLOTS; i++) {
        if (slots[i].used && slots[i].waiting_input) {
            slots[i].waiting_input = false;
            hit = slots[i];
            ms = ms_since_adv(&hit);
            break;
        }
    }
    k_spin_unlock(&lock, key);

    if (hit.used) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(&hit.addr, addr, sizeof(addr));
        LOG_INF("[wake] %s first %s (+%d ms)", addr, what, ms);
    }
}

static int position_listener(const zmk_event_t *eh) {
    if (as_zmk_position_state_changed(eh)->source != ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL) {
        first_input("key");
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(wake_trace, position_listener);
ZMK_SUBSCRIPTION(wake_trace, zmk_position_state_changed);

static void trackball_input(struct input_event *evt, void *user_data) {
    first_input("trackball input");
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(trackball_split)), trackball_input, NULL);

static int wake_trace_init(void) {
    bt_le_scan_cb_register(&scan_cb);
    LOG_INF("[wake] trace enabled");
    return 0;
}

SYS_INIT(wake_trace_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
