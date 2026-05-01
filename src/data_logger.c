/*
 * In-memory ring buffer logger for PMW3610 trackball diagnostics.
 *
 * Captures per-sample sensor data for offline analysis. Designed to run
 * with minimal overhead so the act of logging doesn't perturb the very
 * behavior we're trying to measure.
 *
 * Buffer is volatile RAM only; freeze + dump happens via custom behaviors.
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>

#include "data_logger.h"

LOG_MODULE_REGISTER(dlog, CONFIG_PMW3610_LOG_LEVEL);

#define DLOG_BUF_SAMPLES CONFIG_PMW3610_DATA_LOGGER_BUFFER_SAMPLES

static struct pmw3610_log_entry dlog_buffer[DLOG_BUF_SAMPLES];

/* head: where the next write goes. count: valid samples held. */
static uint32_t dlog_head;
static uint32_t dlog_count;
static atomic_t dlog_frozen = ATOMIC_INIT(0);
static atomic_t dlog_pending_marker = ATOMIC_INIT(PMW3610_DLOG_MARKER_NONE);

void pmw3610_dlog_init(void) {
    dlog_head = 0;
    dlog_count = 0;
    atomic_set(&dlog_frozen, 0);
    atomic_set(&dlog_pending_marker, PMW3610_DLOG_MARKER_NONE);
    memset(dlog_buffer, 0, sizeof(dlog_buffer));
    LOG_INF("init: buf=%u samples, %u bytes", DLOG_BUF_SAMPLES, (uint32_t)sizeof(dlog_buffer));
}

void pmw3610_dlog_push(const struct pmw3610_log_entry *entry) {
    if (atomic_get(&dlog_frozen)) {
        return;
    }
    if (entry == NULL) {
        return;
    }

    /* Direct memory copy, no LOG macros, to keep timing impact minimal. */
    dlog_buffer[dlog_head] = *entry;

    /* Apply pending marker (set by behaviors via set_marker), if any. */
    uint8_t pending = (uint8_t)atomic_get(&dlog_pending_marker);
    if (pending != PMW3610_DLOG_MARKER_NONE) {
        dlog_buffer[dlog_head].marker_id = pending;
        atomic_set(&dlog_pending_marker, PMW3610_DLOG_MARKER_NONE);
    }

    dlog_head = (dlog_head + 1) % DLOG_BUF_SAMPLES;
    if (dlog_count < DLOG_BUF_SAMPLES) {
        dlog_count++;
    }
}

void pmw3610_dlog_freeze(void) {
    atomic_set(&dlog_frozen, 1);
    LOG_INF("frozen, count=%u, head=%u", dlog_count, dlog_head);
}

void pmw3610_dlog_request_dump(void) {
    LOG_INF("dump requested, count=%u", dlog_count);
    pmw3610_dlog_dump_uart();
}

void pmw3610_dlog_clear(void) {
    atomic_set(&dlog_frozen, 0);
    dlog_head = 0;
    dlog_count = 0;
}

bool pmw3610_dlog_is_frozen(void) {
    return atomic_get(&dlog_frozen) != 0;
}

uint32_t pmw3610_dlog_count(void) {
    return dlog_count;
}

bool pmw3610_dlog_get(uint32_t index, struct pmw3610_log_entry *out) {
    if (out == NULL || index >= dlog_count) {
        return false;
    }

    /* Logical index 0 = oldest sample. */
    uint32_t physical;
    if (dlog_count < DLOG_BUF_SAMPLES) {
        physical = index;
    } else {
        physical = (dlog_head + index) % DLOG_BUF_SAMPLES;
    }
    *out = dlog_buffer[physical];
    return true;
}

void pmw3610_dlog_set_marker(uint8_t marker_id) {
    atomic_set(&dlog_pending_marker, (atomic_val_t)marker_id);
}

void pmw3610_dlog_dump_uart(void) {
    uint32_t total = dlog_count;
    LOG_INF("dump_uart: total=%u, frozen=%d", total, (int)atomic_get(&dlog_frozen));
    struct pmw3610_log_entry entry;

    printk("[DLOG_START] %u\n", total);
    printk("device_us,sensor_read_start_us,sensor_read_end_us,filter_start_us,"
           "filter_end_us,ble_send_us,raw_dx,raw_dy,filt_dx,filt_dy,"
           "squal,shutter,battery_mv,ble_conn_interval_units,"
           "filter_state_x,filter_state_y,motion_status,marker_id,flags\n");

    for (uint32_t i = 0; i < total; i++) {
        if (!pmw3610_dlog_get(i, &entry)) {
            break;
        }
        printk("%u,%u,%u,%u,%u,%u,"
               "%d,%d,%d,%d,"
               "%u,%u,%u,%u,"
               "%d,%d,"
               "%u,%u,%u\n",
               entry.device_us,
               entry.sensor_read_start_us,
               entry.sensor_read_end_us,
               entry.filter_start_us,
               entry.filter_end_us,
               entry.ble_send_us,
               entry.raw_dx, entry.raw_dy,
               entry.filt_dx, entry.filt_dy,
               entry.squal, entry.shutter,
               entry.battery_mv, entry.ble_conn_interval_units,
               (int)(entry.filter_state_x * 1000),
               (int)(entry.filter_state_y * 1000),
               entry.motion_status,
               entry.marker_id,
               entry.flags);
    }

    printk("[DLOG_END]\n");
}
