#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Marker IDs (set by &dlog_marker_a / _b behaviors) */
#define PMW3610_DLOG_MARKER_NONE 0x00
#define PMW3610_DLOG_MARKER_A    0x01
#define PMW3610_DLOG_MARKER_B    0x02

/**
 * Per-sample log entry. Packed for compact storage in the ring buffer.
 * Size target: ~56 bytes/entry to keep 1250-sample buffer under 70KB.
 */
struct pmw3610_log_entry {
    uint32_t device_us;            /* k_uptime_get() at sample completion */
    uint32_t sensor_read_start_us; /* Before motion_burst_read */
    uint32_t sensor_read_end_us;   /* After motion_burst_read */
    uint32_t filter_start_us;      /* Before filter block */
    uint32_t filter_end_us;        /* After filter block */
    uint32_t ble_send_us;          /* After input_report_rel (best effort) */

    int16_t  raw_dx;               /* Sensor raw delta X (post 12-bit decode, pre-scale) */
    int16_t  raw_dy;               /* Sensor raw delta Y */
    int16_t  filt_dx;              /* Final dx sent to input_report_rel */
    int16_t  filt_dy;              /* Final dy sent to input_report_rel */

    uint16_t squal;                /* Surface quality (PMW3610 burst byte 4) */
    uint16_t shutter;              /* Auto-exposure shutter (bytes 5,6) */
    uint16_t battery_mv;           /* Battery voltage in mV (0 if unavailable) */
    uint16_t ble_conn_interval_units; /* BLE connection interval in 1.25ms units */

    float    filter_state_x;       /* EMA prev or 1-Euro x_hat */
    float    filter_state_y;

    uint8_t  motion_status;        /* MOTION register byte (buf[0]) */
    uint8_t  marker_id;            /* PMW3610_DLOG_MARKER_* */
    uint8_t  flags;                /* Reserved for future flags */
    uint8_t  _padding;             /* Align to 4 bytes */
};

/* Initialize the ring buffer. Call once at driver init. */
void pmw3610_dlog_init(void);

/* Push one entry into the ring buffer. No-op if frozen. */
void pmw3610_dlog_push(const struct pmw3610_log_entry *entry);

/* Freeze the ring buffer (stops further writes). */
void pmw3610_dlog_freeze(void);

/* Resume recording and reset the buffer. */
void pmw3610_dlog_clear(void);

/* True if the buffer is currently frozen. */
bool pmw3610_dlog_is_frozen(void);

/* Number of valid samples currently held (oldest-to-newest order). */
uint32_t pmw3610_dlog_count(void);

/* Copy entry at logical index [0..count-1] into `out`. Returns true on success. */
bool pmw3610_dlog_get(uint32_t index, struct pmw3610_log_entry *out);

/* Set the marker ID for the next push. Cleared after the next sample. */
void pmw3610_dlog_set_marker(uint8_t marker_id);

#ifdef __cplusplus
}
#endif
