/*
 * BLE GATT service for streaming the in-memory data logger to a host.
 *
 * Service exposes two characteristics:
 *   - request:  Host writes a 1-byte command. 0x01 starts a dump.
 *   - data:     Device notifies header (4B count, LE) + N entries
 *               (sizeof(struct pmw3610_log_entry) each) + footer (4B 0xFF).
 *
 * Dump runs on a workqueue to avoid blocking the BLE host context.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "data_logger.h"

LOG_MODULE_REGISTER(dlog_ble, CONFIG_INPUT_LOG_LEVEL);

/* Random 128-bit UUIDs; no overlap with ZMK Studio service. */
#define DLOG_SVC_UUID_VAL \
    BT_UUID_128_ENCODE(0xa1b2c3d0, 0x1111, 0x4222, 0x8333, 0x4444aabbccdd)
#define DLOG_CHR_REQ_UUID_VAL \
    BT_UUID_128_ENCODE(0xa1b2c3d1, 0x1111, 0x4222, 0x8333, 0x4444aabbccdd)
#define DLOG_CHR_DATA_UUID_VAL \
    BT_UUID_128_ENCODE(0xa1b2c3d2, 0x1111, 0x4222, 0x8333, 0x4444aabbccdd)

static struct bt_uuid_128 dlog_svc_uuid = BT_UUID_INIT_128(DLOG_SVC_UUID_VAL);
static struct bt_uuid_128 dlog_chr_req_uuid = BT_UUID_INIT_128(DLOG_CHR_REQ_UUID_VAL);
static struct bt_uuid_128 dlog_chr_data_uuid = BT_UUID_INIT_128(DLOG_CHR_DATA_UUID_VAL);

static atomic_t dump_in_progress = ATOMIC_INIT(0);
static struct bt_conn *dump_conn;
static struct k_work dump_work;

static void dump_work_handler(struct k_work *work);

static void data_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    LOG_INF("dlog CCC changed to 0x%04x", value);
}

/* Host writes 0x01 to start a dump. */
static ssize_t request_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (offset != 0 || len != 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    uint8_t cmd = ((const uint8_t *)buf)[0];
    if (cmd == 0x01) {
        if (atomic_set(&dump_in_progress, 1) == 0) {
            if (dump_conn) {
                bt_conn_unref(dump_conn);
            }
            dump_conn = bt_conn_ref(conn);
            k_work_submit(&dump_work);
        }
    }
    return len;
}

BT_GATT_SERVICE_DEFINE(
    dlog_svc,
    BT_GATT_PRIMARY_SERVICE(&dlog_svc_uuid),
    BT_GATT_CHARACTERISTIC(&dlog_chr_req_uuid.uuid, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE,
                           NULL, request_write, NULL),
    BT_GATT_CHARACTERISTIC(&dlog_chr_data_uuid.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL,
                           NULL, NULL),
    BT_GATT_CCC(data_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

/* The data-characteristic value attribute is the 4th entry in the service
 * (indices: 0=svc, 1=req-decl, 2=req-val, 3=data-decl, 4=data-val, 5=ccc). */
#define DLOG_DATA_ATTR_INDEX 4

static void dump_work_handler(struct k_work *work) {
    if (!dump_conn) {
        atomic_set(&dump_in_progress, 0);
        return;
    }

    const struct bt_gatt_attr *data_attr = &dlog_svc.attrs[DLOG_DATA_ATTR_INDEX];
    uint32_t total = pmw3610_dlog_count();

    LOG_INF("dlog dump start: %u entries", total);

    /* Header: little-endian uint32 total count */
    uint8_t header[4] = {
        (uint8_t)(total & 0xFF),
        (uint8_t)((total >> 8) & 0xFF),
        (uint8_t)((total >> 16) & 0xFF),
        (uint8_t)((total >> 24) & 0xFF),
    };
    bt_gatt_notify(dump_conn, data_attr, header, sizeof(header));
    k_msleep(2);

    struct pmw3610_log_entry entry;
    for (uint32_t i = 0; i < total; i++) {
        if (!pmw3610_dlog_get(i, &entry)) {
            break;
        }
        int retries = 0;
        while (retries < 50) {
            int err = bt_gatt_notify(dump_conn, data_attr, &entry, sizeof(entry));
            if (err == 0) {
                break;
            } else if (err == -ENOMEM || err == -EAGAIN) {
                k_msleep(5);
                retries++;
                continue;
            }
            LOG_WRN("dlog notify failed at %u: %d", i, err);
            break;
        }
        k_msleep(2);
    }

    /* Footer: 4 bytes of 0xFF as end-of-stream sentinel */
    uint8_t footer[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    bt_gatt_notify(dump_conn, data_attr, footer, sizeof(footer));

    LOG_INF("dlog dump complete");

    bt_conn_unref(dump_conn);
    dump_conn = NULL;
    atomic_set(&dump_in_progress, 0);
}

/* Behavior-triggered dump request. Logs only; the actual dump is started
 * when the host writes 0x01 to the request characteristic, since CCC
 * tracking per-connection is non-trivial here. */
void pmw3610_dlog_request_dump(void) {
    LOG_INF("dlog: behavior dump requested. Host should write 0x01 to request char.");
}

static int dlog_ble_init(void) {
    k_work_init(&dump_work, dump_work_handler);
    LOG_INF("Data logger BLE service initialized");
    return 0;
}

SYS_INIT(dlog_ble_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
