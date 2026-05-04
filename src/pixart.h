#pragma once

/**
 * @file pixart.h
 *
 * @brief Common header file for all optical motion sensor by PIXART
 */

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

enum pixart_input_mode { MOVE = 0, SCROLL, SNIPE };

/* device data structure */
struct pixart_data {
    const struct device *dev;

    enum pixart_input_mode curr_mode;
    uint32_t curr_cpi;
    int32_t scroll_delta_x;
    int32_t scroll_delta_y;

#ifdef CONFIG_PMW3610_POLLING_RATE_125_SW
    int64_t last_poll_time;
    int16_t last_x;
    int16_t last_y;
#endif

    // motion interrupt isr
    struct gpio_callback irq_gpio_cb;
    // the work structure holding the trigger job
    struct k_work trigger_work;

    // the work structure for delayable init steps
    struct k_work_delayable init_work;
    int async_init_step;

    //
    bool ready;           // whether init is finished successfully
    bool last_read_burst; // todo: needed?
    int err;              // error code during async init

    // for pmw3610 smart algorithm
    bool sw_smart_flag;

#if CONFIG_PMW3610_DEADZONE > 0
    int64_t last_move_time;
    uint8_t dz_consec_count;
    bool    dz_grace_activated;
#endif

    float move_remainder_x;
    float move_remainder_y;

#ifdef CONFIG_PMW3610_FILTER_EMA
    int16_t ema_x;
    int16_t ema_y;
    bool ema_initialized;
#endif

#ifdef CONFIG_PMW3610_FILTER_1EURO
    float euro_x_pos;
    float euro_y_pos;
    float euro_x_prev;
    float euro_y_prev;
    float euro_x_dx_prev;
    float euro_y_dx_prev;
    float euro_x_last_out;
    float euro_y_last_out;
    float euro_x_remainder;
    float euro_y_remainder;
    int64_t euro_t_prev;
    bool euro_initialized;
#endif

#ifdef CONFIG_PMW3610_DATA_LOGGER
    /* Pending marker (set by &dlog_marker_a / _b behavior, consumed by next sample) */
    uint8_t dlog_pending_marker;
    /* Auto-freeze tracking */
    int64_t dlog_low_velocity_since_ms;
#endif
};

// device config data structure
struct pixart_config {
    struct gpio_dt_spec irq_gpio;
    struct spi_dt_spec bus;
    struct gpio_dt_spec cs_gpio;
    size_t scroll_layers_len;
    int32_t *scroll_layers;
    size_t snipe_layers_len;
    int32_t *snipe_layers;
};

#ifdef __cplusplus
}
#endif

/**
 * @}
 */
