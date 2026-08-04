#include <errno.h>
#include <stdbool.h>

#ifdef TRACKBALL_PROFILE_HOST_TEST
#include <stdio.h>
#else
#include <zephyr/ztest.h>
#endif

#include <pmw3610/trackball_profile.h>

#ifdef TRACKBALL_PROFILE_HOST_TEST

#define ASSERT_OK(expression)                                                                   \
    do {                                                                                        \
        int result = (expression);                                                              \
        if (result != 0) {                                                                      \
            fprintf(stderr, "%s returned %d, expected 0\\n", #expression, result);           \
            return 1;                                                                           \
        }                                                                                       \
    } while (0)

#define ASSERT_EQUAL(expected, expression)                                                     \
    do {                                                                                        \
        int result = (expression);                                                              \
        if (result != (expected)) {                                                            \
            fprintf(stderr, "%s returned %d, expected %d\\n", #expression, result, expected); \
            return 1;                                                                           \
        }                                                                                       \
    } while (0)

static int test_accepts_200_step_values(void) {
    ASSERT_OK(trackball_profile_validate(800, 200));
    ASSERT_OK(trackball_profile_validate(3200, 3200));

    return 0;
}

static int test_rejects_invalid_values(void) {
    ASSERT_EQUAL(-EINVAL, trackball_profile_validate(799, 200));
    ASSERT_EQUAL(-EINVAL, trackball_profile_validate(800, 4000));
    ASSERT_EQUAL(-EINVAL, trackball_profile_validate(400, 800));

    return 0;
}

static int test_mode_selects_confirmed_cpi(void) {
    struct trackball_profile profile = {.normal_cpi = 800, .precision_cpi = 200};

    ASSERT_EQUAL(800, trackball_profile_cpi(&profile, false));
    ASSERT_EQUAL(200, trackball_profile_cpi(&profile, true));

    return 0;
}

int main(void) {
    return test_accepts_200_step_values() || test_rejects_invalid_values() ||
           test_mode_selects_confirmed_cpi();
}

#else

ZTEST(trackball_profile, test_accepts_200_step_values) {
    zassert_ok(trackball_profile_validate(800, 200));
    zassert_ok(trackball_profile_validate(3200, 3200));
}

ZTEST(trackball_profile, test_rejects_invalid_values) {
    zassert_equal(-EINVAL, trackball_profile_validate(799, 200));
    zassert_equal(-EINVAL, trackball_profile_validate(800, 4000));
    zassert_equal(-EINVAL, trackball_profile_validate(400, 800));
}

ZTEST(trackball_profile, test_mode_selects_confirmed_cpi) {
    struct trackball_profile profile = {.normal_cpi = 800, .precision_cpi = 200};

    zassert_equal(800, trackball_profile_cpi(&profile, false));
    zassert_equal(200, trackball_profile_cpi(&profile, true));
}

ZTEST_SUITE(trackball_profile, NULL, NULL, NULL, NULL, NULL);

#endif
