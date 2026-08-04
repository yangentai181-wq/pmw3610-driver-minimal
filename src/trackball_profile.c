#include <errno.h>
#include <stddef.h>

#include <pmw3610/trackball_profile.h>

int trackball_profile_validate(uint16_t normal_cpi, uint16_t precision_cpi) {
    if (normal_cpi < TRACKBALL_PROFILE_MIN_CPI || normal_cpi > TRACKBALL_PROFILE_MAX_CPI ||
        precision_cpi < TRACKBALL_PROFILE_MIN_CPI || precision_cpi > TRACKBALL_PROFILE_MAX_CPI ||
        normal_cpi % TRACKBALL_PROFILE_CPI_STEP != 0U ||
        precision_cpi % TRACKBALL_PROFILE_CPI_STEP != 0U || precision_cpi > normal_cpi) {
        return -EINVAL;
    }

    return 0;
}

uint16_t trackball_profile_cpi(const struct trackball_profile *profile, bool precision_active) {
    if (profile == NULL) {
        return 0;
    }

    return precision_active ? profile->precision_cpi : profile->normal_cpi;
}
