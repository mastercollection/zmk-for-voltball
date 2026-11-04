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
    bool adv_logged;
    bool waiting_input;
    bt_addr_le_t addr;
    int64_t t_adv;
};

static struct trace_slot slots[TRACE_SLOTS];
static struct k_spinlock lock;

static const uint8_t split_uuid[16] = {ZMK_SPLIT_BT_SERVICE_UUID};

static struct trace_slot *slot_for(const bt_addr_le_t *addr, bool create) {
    struct trace_slot *free_slot = NULL;
    for (int i = 0; i < TRACE_SLOTS; i++) {
        if (slots[i].used && bt_addr_le_eq(&slots[i].addr, addr)) {
            return &slots[i];
        }
        if (!slots[i].used && !free_slot) {
            free_slot = &slots[i];
        }
    }
    if (!create || !free_slot) {
        return NULL;
    }
    memset(free_slot, 0, sizeof(*free_slot));
    free_slot->used = true;
    bt_addr_le_copy(&free_slot->addr, addr);
    return free_slot;
}

static int64_t since_adv(const struct trace_slot *s, int64_t now) {
    return s->t_adv ? now - s->t_adv : -1;
}

static bool ad_has_split_uuid(struct net_buf_simple *ad) {
    while (ad->len > 1) {
        uint8_t len = net_buf_simple_pull_u8(ad);
        if (len == 0 || len > ad->len) {
            return false;
        }
        uint8_t type = net_buf_simple_pull_u8(ad);
        const uint8_t *data = net_buf_simple_pull_mem(ad, len - 1);
        if (type == BT_DATA_UUID128_ALL || type == BT_DATA_UUID128_SOME) {
            for (int off = 0; off + 16 <= len - 1; off += 16) {
                if (memcmp(data + off, split_uuid, 16) == 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

static void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf) {
    /* A bonded half sends directed advertising (no AD data) at the dongle,
     * same as ZMK's split central accepts; an unbonded one sends the split UUID. */
    bool directed = info->adv_type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND;
    bool split = directed;
    if (!split && info->adv_type == BT_GAP_ADV_TYPE_ADV_IND) {
        struct net_buf_simple_state state;
        net_buf_simple_save(buf, &state);
        split = ad_has_split_uuid(buf);
        net_buf_simple_restore(buf, &state);
    }
    if (!split) {
        return;
    }

    int64_t now = k_uptime_get();
    bool log = false;
    k_spinlock_key_t key = k_spin_lock(&lock);
    struct trace_slot *s = slot_for(info->addr, true);
    if (s && !s->connected && !s->adv_logged) {
        s->adv_logged = true;
        s->t_adv = now;
        log = true;
    }
    k_spin_unlock(&lock, key);

    if (log) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(info->addr, addr, sizeof(addr));
        LOG_INF("[wake] %s adv seen (t=0, %s, rssi %d)", addr,
                directed ? "directed" : "split uuid", info->rssi);
    }
}

static struct bt_le_scan_cb scan_cb = {.recv = scan_recv};

static bool is_split_conn(struct bt_conn *conn) {
    struct bt_conn_info info;
    return bt_conn_get_info(conn, &info) == 0 && info.role == BT_CONN_ROLE_CENTRAL;
}

static void on_connected(struct bt_conn *conn, uint8_t err) {
    if (!is_split_conn(conn)) {
        return;
    }
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    if (err) {
        LOG_INF("[wake] %s connect failed (err 0x%02x)", addr, err);
        return;
    }

    int64_t now = k_uptime_get();
    int64_t dt;
    k_spinlock_key_t key = k_spin_lock(&lock);
    struct trace_slot *s = slot_for(bt_conn_get_dst(conn), true);
    if (s) {
        s->connected = true;
        s->waiting_input = true;
    }
    dt = s ? since_adv(s, now) : -1;
    k_spin_unlock(&lock, key);

    LOG_INF("[wake] %s connected (+%d ms)", addr, (int)dt);
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason) {
    if (!is_split_conn(conn)) {
        return;
    }
    k_spinlock_key_t key = k_spin_lock(&lock);
    struct trace_slot *s = slot_for(bt_conn_get_dst(conn), false);
    if (s) {
        s->connected = false;
        s->adv_logged = false;
        s->waiting_input = false;
        s->t_adv = 0;
    }
    k_spin_unlock(&lock, key);

    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("[wake] %s disconnected (reason 0x%02x)", addr, reason);
}

static void on_security_changed(struct bt_conn *conn, bt_security_t level,
                                enum bt_security_err err) {
    if (!is_split_conn(conn)) {
        return;
    }
    int64_t now = k_uptime_get();
    k_spinlock_key_t key = k_spin_lock(&lock);
    struct trace_slot *s = slot_for(bt_conn_get_dst(conn), false);
    int64_t dt = s ? since_adv(s, now) : -1;
    k_spin_unlock(&lock, key);

    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("[wake] %s security level %d err %d (+%d ms)", addr, level, err, (int)dt);
}

static void on_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency,
                             uint16_t timeout) {
    if (!is_split_conn(conn)) {
        return;
    }
    int64_t now = k_uptime_get();
    k_spinlock_key_t key = k_spin_lock(&lock);
    struct trace_slot *s = slot_for(bt_conn_get_dst(conn), false);
    int64_t dt = s ? since_adv(s, now) : -1;
    k_spin_unlock(&lock, key);

    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    /* interval is in 1.25 ms units */
    LOG_INF("[wake] %s conn interval %u.%02u ms latency %u (+%d ms)", addr,
            (interval * 125) / 100, (interval * 125) % 100, latency, (int)dt);
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
    int64_t now = k_uptime_get();
    int64_t dt = -1;
    bool log = false;
    char addr[BT_ADDR_LE_STR_LEN];

    k_spinlock_key_t key = k_spin_lock(&lock);
    for (int i = 0; i < TRACE_SLOTS; i++) {
        if (slots[i].used && slots[i].waiting_input) {
            slots[i].waiting_input = false;
            dt = since_adv(&slots[i], now);
            bt_addr_le_to_str(&slots[i].addr, addr, sizeof(addr));
            log = true;
            break;
        }
    }
    k_spin_unlock(&lock, key);

    if (log) {
        LOG_INF("[wake] %s first %s (+%d ms)", addr, what, (int)dt);
    }
}

static int position_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev && ev->source != ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL) {
        first_input("key");
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(wake_trace, position_listener);
ZMK_SUBSCRIPTION(wake_trace, zmk_position_state_changed);

#if DT_NODE_EXISTS(DT_NODELABEL(trackball_split))
static void trackball_input(struct input_event *evt, void *user_data) {
    ARG_UNUSED(evt);
    ARG_UNUSED(user_data);
    first_input("trackball input");
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(trackball_split)), trackball_input, NULL);
#endif

static int wake_trace_init(void) {
    bt_le_scan_cb_register(&scan_cb);
    LOG_INF("[wake] trace enabled");
    return 0;
}

SYS_INIT(wake_trace_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
