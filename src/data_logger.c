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

#include "data_logger.h"

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
