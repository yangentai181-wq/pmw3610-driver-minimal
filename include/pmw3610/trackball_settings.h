#pragma once

#include <stdbool.h>
#include <stdint.h>

#if defined(TRACKBALL_SETTINGS_HOST_TEST) || defined(TRACKBALL_SETTINGS_TEST_ADAPTER)
struct zmk_behavior_binding {
    const char *behavior_dev;
    uint32_t param1;
    uint32_t param2;
};

#define TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_KP 1U
#define TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_LT 2U
#define TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_MT 3U
#define TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_LT_MKP 4U
#else
#include <zmk/behavior.h>
#endif

#include <pmw3610/trackball_profile.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRACKBALL_SETTINGS_SCHEMA_VERSION 1U
#define TRACKBALL_SETTINGS_BASE_LAYER 0U
#define TRACKBALL_SETTINGS_PRECISION_LAYER 8U

struct trackball_settings_record {
    uint8_t schema_version;
    bool enabled;
    uint8_t selected_position;
    uint16_t normal_cpi;
    uint16_t precision_cpi;
    uint16_t original_behavior_id;
    uint32_t original_param1;
    uint32_t original_param2;
    uint32_t revision;
};

struct trackball_settings_request {
    uint16_t normal_cpi;
    uint16_t precision_cpi;
    bool enabled;
    uint8_t selected_position;
    uint32_t expected_revision;
};

struct trackball_settings_adapter {
    const struct zmk_behavior_binding *(*get_binding)(uint8_t layer, uint8_t position);
    int (*set_binding)(uint8_t layer, uint8_t position, struct zmk_behavior_binding binding);
    int (*save_keymap)(void);
    int (*save_settings)(const struct trackball_settings_record *record);
    int (*apply_profile)(const struct trackball_profile *profile);
};

/* Pure transaction operations for the Studio RPC handler and unit tests. */
int trackball_settings_validate(const struct trackball_settings_record *current,
                                const struct trackball_settings_request *request,
                                const struct trackball_settings_adapter *adapter);
int trackball_settings_apply(struct trackball_settings_record *current,
                             const struct trackball_settings_request *request,
                             const struct trackball_settings_adapter *adapter);
int trackball_settings_reload(const struct trackball_settings_record *record,
                              const struct trackball_settings_adapter *adapter);

/* Firmware-owned state and persistence entry points. */
int trackball_settings_get_record(struct trackball_settings_record *record);
int trackball_settings_validate_request(const struct trackball_settings_request *request);
int trackball_settings_apply_request(const struct trackball_settings_request *request);

#ifdef __cplusplus
}
#endif
