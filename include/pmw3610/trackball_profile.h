#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRACKBALL_PROFILE_MIN_CPI 200U
#define TRACKBALL_PROFILE_MAX_CPI 3200U
#define TRACKBALL_PROFILE_CPI_STEP 200U

struct trackball_profile {
    uint16_t normal_cpi;
    uint16_t precision_cpi;
};

int trackball_profile_validate(uint16_t normal_cpi, uint16_t precision_cpi);
uint16_t trackball_profile_cpi(const struct trackball_profile *profile, bool precision_active);

/* Runtime sensor APIs must be called from Zephyr thread context, not an ISR. */
int pmw3610_apply_profile(const struct trackball_profile *profile);
int pmw3610_set_precision_active(bool active);
uint16_t pmw3610_current_cpi(void);

#ifdef __cplusplus
}
#endif
