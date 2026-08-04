#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef TRACKBALL_SETTINGS_HOST_TEST
#include <stdio.h>
#else
#include <zephyr/ztest.h>
#endif

#include <pmw3610/trackball_settings.h>

#define TEST_POSITION_COUNT 12U
#define KEY_A 0x00070004U
#define KEY_B 0x00070005U
#define KEY_C 0x00070006U
#define MOD_LSFT 0x00000002U
#define MOUSE_BUTTON_1 0x00000001U

enum fake_operation {
    FAKE_OPERATION_SET_BINDING,
    FAKE_OPERATION_APPLY_PROFILE,
    FAKE_OPERATION_SAVE_KEYMAP,
    FAKE_OPERATION_SAVE_SETTINGS,
};

struct fake_state {
    struct zmk_behavior_binding bindings[TEST_POSITION_COUNT];
    bool binding_present[TEST_POSITION_COUNT];
    struct trackball_profile profile;
    struct trackball_settings_record persisted;
    int set_calls;
    int save_keymap_calls;
    int save_settings_calls;
    int apply_profile_calls;
    int fail_set_call;
    int fail_save_keymap_call;
    int fail_save_settings_call;
    int fail_apply_profile_call;
    int set_error;
    int save_keymap_error;
    int save_settings_error;
    int apply_profile_error;
    enum fake_operation operations[32];
    int operation_count;
    uint8_t set_positions[16];
    int set_position_count;
};

static struct fake_state fake;

static struct zmk_behavior_binding binding(const char *behavior, uint32_t param1, uint32_t param2) {
    return (struct zmk_behavior_binding){
        .behavior_dev = behavior,
        .param1 = param1,
        .param2 = param2,
    };
}

static bool binding_equal(const struct zmk_behavior_binding *actual,
                          const struct zmk_behavior_binding *expected) {
    return actual != NULL && expected != NULL && actual->behavior_dev != NULL &&
           expected->behavior_dev != NULL && strcmp(actual->behavior_dev, expected->behavior_dev) == 0 &&
           actual->param1 == expected->param1 && actual->param2 == expected->param2;
}

static bool record_equal(const struct trackball_settings_record *actual,
                         const struct trackball_settings_record *expected) {
    return actual->schema_version == expected->schema_version && actual->enabled == expected->enabled &&
           actual->selected_position == expected->selected_position &&
           actual->normal_cpi == expected->normal_cpi && actual->precision_cpi == expected->precision_cpi &&
           actual->original_behavior_id == expected->original_behavior_id &&
           actual->original_param1 == expected->original_param1 &&
           actual->original_param2 == expected->original_param2 && actual->revision == expected->revision;
}

static void record_operation(enum fake_operation operation) {
    if (fake.operation_count < (int)(sizeof(fake.operations) / sizeof(fake.operations[0]))) {
        fake.operations[fake.operation_count++] = operation;
    }
}

static const struct zmk_behavior_binding *fake_get_binding(uint8_t layer, uint8_t position) {
    if (layer != TRACKBALL_SETTINGS_BASE_LAYER || position >= TEST_POSITION_COUNT ||
        !fake.binding_present[position]) {
        return NULL;
    }

    return &fake.bindings[position];
}

static int fake_set_binding(uint8_t layer, uint8_t position, struct zmk_behavior_binding value) {
    fake.set_calls++;
    record_operation(FAKE_OPERATION_SET_BINDING);
    if (fake.set_position_count < (int)(sizeof(fake.set_positions) / sizeof(fake.set_positions[0]))) {
        fake.set_positions[fake.set_position_count++] = position;
    }
    if (fake.fail_set_call == fake.set_calls) {
        return fake.set_error;
    }
    if (layer != TRACKBALL_SETTINGS_BASE_LAYER || position >= TEST_POSITION_COUNT) {
        return -EINVAL;
    }

    fake.bindings[position] = value;
    fake.binding_present[position] = true;
    return 0;
}

static int fake_save_keymap(void) {
    fake.save_keymap_calls++;
    record_operation(FAKE_OPERATION_SAVE_KEYMAP);
    if (fake.fail_save_keymap_call == fake.save_keymap_calls) {
        return fake.save_keymap_error;
    }

    return 0;
}

static int fake_save_settings(const struct trackball_settings_record *record) {
    fake.save_settings_calls++;
    record_operation(FAKE_OPERATION_SAVE_SETTINGS);
    if (fake.fail_save_settings_call == fake.save_settings_calls) {
        return fake.save_settings_error;
    }

    fake.persisted = *record;
    return 0;
}

static int fake_apply_profile(const struct trackball_profile *profile) {
    fake.apply_profile_calls++;
    record_operation(FAKE_OPERATION_APPLY_PROFILE);
    if (fake.fail_apply_profile_call == fake.apply_profile_calls) {
        return fake.apply_profile_error;
    }

    fake.profile = *profile;
    return 0;
}

static const struct trackball_settings_adapter adapter = {
    .get_binding = fake_get_binding,
    .set_binding = fake_set_binding,
    .save_keymap = fake_save_keymap,
    .save_settings = fake_save_settings,
    .apply_profile = fake_apply_profile,
};

static struct trackball_settings_record disabled_record(uint32_t revision) {
    return (struct trackball_settings_record){
        .schema_version = TRACKBALL_SETTINGS_SCHEMA_VERSION,
        .enabled = false,
        .selected_position = 0,
        .normal_cpi = 800,
        .precision_cpi = 200,
        .original_behavior_id = 0,
        .original_param1 = 0,
        .original_param2 = 0,
        .revision = revision,
    };
}

static struct trackball_settings_request request(uint16_t normal_cpi, uint16_t precision_cpi,
                                                 bool enabled, uint8_t selected_position,
                                                 uint32_t expected_revision) {
    return (struct trackball_settings_request){
        .normal_cpi = normal_cpi,
        .precision_cpi = precision_cpi,
        .enabled = enabled,
        .selected_position = selected_position,
        .expected_revision = expected_revision,
    };
}

static void fake_reset(const struct trackball_settings_record *current) {
    memset(&fake, 0, sizeof(fake));
    fake.profile = (struct trackball_profile){.normal_cpi = current->normal_cpi,
                                               .precision_cpi = current->precision_cpi};
    fake.persisted = *current;
    fake.set_error = -EIO;
    fake.save_keymap_error = -EBUSY;
    fake.save_settings_error = -ENOSPC;
    fake.apply_profile_error = -EAGAIN;
}

static void fake_put_binding(uint8_t position, struct zmk_behavior_binding value) {
    fake.bindings[position] = value;
    fake.binding_present[position] = true;
}

#ifdef TRACKBALL_SETTINGS_HOST_TEST

#define CHECK(condition, message)                                                                 \
    do {                                                                                          \
        if (!(condition)) {                                                                       \
            fprintf(stderr, "%s:%d: %s\\n", __func__, __LINE__, message);                      \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

#define CHECK_INT(expected, actual)                                                               \
    do {                                                                                          \
        int actual_value = (actual);                                                             \
        if (actual_value != (expected)) {                                                        \
            fprintf(stderr, "%s:%d: expected %d, got %d\\n", __func__, __LINE__, (expected),    \
                    actual_value);                                                               \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

#else

#define CHECK(condition, message)                                                                 \
    do {                                                                                          \
        (void)(message);                                                                          \
        if (!(condition)) {                                                                       \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

#define CHECK_INT(expected, actual)                                                               \
    do {                                                                                          \
        if ((actual) != (expected)) {                                                            \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

#endif

static int check_no_writes(void) {
    CHECK_INT(0, fake.set_calls);
    CHECK_INT(0, fake.apply_profile_calls);
    CHECK_INT(0, fake.save_keymap_calls);
    CHECK_INT(0, fake.save_settings_calls);
    return 0;
}

static int test_enables_kp_and_stores_exact_original_binding(void) {
    struct trackball_settings_record current = disabled_record(7);
    struct trackball_settings_request next = request(1200, 400, true, 5, 7);
    struct zmk_behavior_binding original = binding("kp", KEY_A, 0);
    struct zmk_behavior_binding expected_wrapper = binding("lt", TRACKBALL_SETTINGS_PRECISION_LAYER, KEY_A);

    fake_reset(&current);
    fake_put_binding(5, original);

    CHECK_INT(0, trackball_settings_apply(&current, &next, &adapter));
    CHECK(current.enabled, "the successful record must be enabled");
    CHECK_INT(5, current.selected_position);
    CHECK_INT(1200, current.normal_cpi);
    CHECK_INT(400, current.precision_cpi);
    CHECK(current.original_behavior_id != 0, "the original behavior must be persisted by local ID");
    CHECK_INT((int)KEY_A, (int)current.original_param1);
    CHECK_INT(0, (int)current.original_param2);
    CHECK_INT(8, (int)current.revision);
    CHECK(binding_equal(fake_get_binding(0, 5), &expected_wrapper),
          "a plain key must be wrapped as &lt 8 <tap>");
    CHECK_INT(1200, fake.profile.normal_cpi);
    CHECK_INT(400, fake.profile.precision_cpi);
    CHECK(record_equal(&fake.persisted, &current), "the committed record must be the saved record");
    CHECK_INT(1, fake.save_keymap_calls);
    CHECK_INT(1, fake.save_settings_calls);
    return 0;
}

static int test_enables_lt_and_preserves_tap_param2(void) {
    struct trackball_settings_record current = disabled_record(3);
    struct trackball_settings_request next = request(800, 200, true, 2, 3);
    struct zmk_behavior_binding original = binding("lt", 3, KEY_B);
    struct zmk_behavior_binding expected_wrapper = binding("lt", TRACKBALL_SETTINGS_PRECISION_LAYER, KEY_B);

    fake_reset(&current);
    fake_put_binding(2, original);

    CHECK_INT(0, trackball_settings_apply(&current, &next, &adapter));
    CHECK(binding_equal(fake_get_binding(0, 2), &expected_wrapper),
          "a layer-tap must retain its complete tap parameter");
    CHECK_INT(3, (int)current.original_param1);
    CHECK_INT((int)KEY_B, (int)current.original_param2);
    return 0;
}

static int test_enables_mt_and_preserves_tap_param2(void) {
    struct trackball_settings_record current = disabled_record(3);
    struct trackball_settings_request next = request(800, 200, true, 2, 3);
    struct zmk_behavior_binding original = binding("mt", MOD_LSFT, KEY_C);
    struct zmk_behavior_binding expected_wrapper = binding("lt", TRACKBALL_SETTINGS_PRECISION_LAYER, KEY_C);

    fake_reset(&current);
    fake_put_binding(2, original);

    CHECK_INT(0, trackball_settings_apply(&current, &next, &adapter));
    CHECK(binding_equal(fake_get_binding(0, 2), &expected_wrapper),
          "a mod-tap must retain its complete tap parameter");
    CHECK_INT((int)MOD_LSFT, (int)current.original_param1);
    CHECK_INT((int)KEY_C, (int)current.original_param2);
    return 0;
}

static int test_enables_lt_mkp_and_preserves_mouse_tap(void) {
    struct trackball_settings_record current = disabled_record(9);
    struct trackball_settings_request next = request(1000, 400, true, 7, 9);
    struct zmk_behavior_binding original = binding("lt_mkp", 1, MOUSE_BUTTON_1);
    struct zmk_behavior_binding expected_wrapper =
        binding("lt_mkp", TRACKBALL_SETTINGS_PRECISION_LAYER, MOUSE_BUTTON_1);

    fake_reset(&current);
    fake_put_binding(7, original);

    CHECK_INT(0, trackball_settings_apply(&current, &next, &adapter));
    CHECK(binding_equal(fake_get_binding(0, 7), &expected_wrapper),
          "the mouse layer-tap must retain its mouse-button tap behavior");
    CHECK_INT((int)MOUSE_BUTTON_1, (int)current.original_param2);
    return 0;
}

static int test_changing_positions_restores_previous_binding_before_wrapping_new_one(void) {
    struct trackball_settings_record current = {
        .schema_version = TRACKBALL_SETTINGS_SCHEMA_VERSION,
        .enabled = true,
        .selected_position = 1,
        .normal_cpi = 800,
        .precision_cpi = 200,
        .original_behavior_id = TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_KP,
        .original_param1 = KEY_A,
        .original_param2 = 0,
        .revision = 11,
    };
    struct trackball_settings_request next = request(1000, 400, true, 3, 11);
    struct zmk_behavior_binding former_original = binding("kp", KEY_A, 0);
    struct zmk_behavior_binding new_original = binding("mt", MOD_LSFT, KEY_B);
    struct zmk_behavior_binding former_wrapper = binding("lt", TRACKBALL_SETTINGS_PRECISION_LAYER, KEY_A);
    struct zmk_behavior_binding new_wrapper = binding("lt", TRACKBALL_SETTINGS_PRECISION_LAYER, KEY_B);

    fake_reset(&current);
    fake_put_binding(1, former_wrapper);
    fake_put_binding(3, new_original);

    CHECK_INT(0, trackball_settings_apply(&current, &next, &adapter));
    CHECK(binding_equal(fake_get_binding(0, 1), &former_original),
          "moving the key must restore the former complete original binding");
    CHECK(binding_equal(fake_get_binding(0, 3), &new_wrapper),
          "moving the key must wrap the newly selected binding");
    CHECK_INT(2, fake.set_position_count);
    CHECK_INT(1, fake.set_positions[0]);
    CHECK_INT(3, fake.set_positions[1]);
    CHECK_INT((int)MOD_LSFT, (int)current.original_param1);
    CHECK_INT((int)KEY_B, (int)current.original_param2);
    return 0;
}

static int test_disabling_restores_original_binding_and_normal_profile(void) {
    struct trackball_settings_record current = {
        .schema_version = TRACKBALL_SETTINGS_SCHEMA_VERSION,
        .enabled = true,
        .selected_position = 4,
        .normal_cpi = 800,
        .precision_cpi = 200,
        .original_behavior_id = TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_KP,
        .original_param1 = KEY_C,
        .original_param2 = 0,
        .revision = 14,
    };
    struct trackball_settings_request next = request(1000, 400, false, 0, 14);
    struct zmk_behavior_binding original = binding("kp", KEY_C, 0);
    struct zmk_behavior_binding wrapper = binding("lt", TRACKBALL_SETTINGS_PRECISION_LAYER, KEY_C);

    fake_reset(&current);
    fake_put_binding(4, wrapper);

    CHECK_INT(0, trackball_settings_apply(&current, &next, &adapter));
    CHECK(!current.enabled, "disabling must clear enabled state");
    CHECK(binding_equal(fake_get_binding(0, 4), &original),
          "disabling must restore the original selected binding");
    CHECK_INT(1000, fake.profile.normal_cpi);
    CHECK_INT(400, fake.profile.precision_cpi);
    CHECK_INT(15, (int)current.revision);
    return 0;
}

static int test_rejects_unsupported_and_precision_layer_bindings_without_writes(void) {
    struct trackball_settings_record current = disabled_record(2);
    struct trackball_settings_request next = request(800, 200, true, 0, 2);
    struct trackball_settings_record before = current;

    fake_reset(&current);
    fake_put_binding(0, binding("trans", 0, 0));
    CHECK_INT(-ENOTSUP, trackball_settings_apply(&current, &next, &adapter));
    CHECK(record_equal(&before, &current), "unsupported bindings must not mutate the record");
    CHECK_INT(0, check_no_writes());

    fake_reset(&current);
    fake_put_binding(0, binding("lt", TRACKBALL_SETTINGS_PRECISION_LAYER, KEY_A));
    CHECK_INT(-ENOTSUP, trackball_settings_apply(&current, &next, &adapter));
    CHECK(record_equal(&before, &current), "layer 8 references must not mutate the record");
    CHECK_INT(0, check_no_writes());
    return 0;
}

static int test_rejects_stale_revision_without_writes(void) {
    struct trackball_settings_record current = disabled_record(6);
    struct trackball_settings_request next = request(800, 200, true, 0, 5);
    struct trackball_settings_record before = current;

    fake_reset(&current);
    fake_put_binding(0, binding("kp", KEY_A, 0));

    CHECK_INT(-ESTALE, trackball_settings_apply(&current, &next, &adapter));
    CHECK(record_equal(&before, &current), "a stale request must not mutate the record");
    CHECK_INT(0, check_no_writes());
    return 0;
}

static int test_rejects_an_invalid_current_record_without_writes(void) {
    struct trackball_settings_record current = disabled_record(6);
    struct trackball_settings_request next = request(800, 200, true, 0, 6);
    struct trackball_settings_record before;

    current.normal_cpi = 750;
    before = current;
    fake_reset(&current);
    fake_put_binding(0, binding("kp", KEY_A, 0));

    CHECK_INT(-EINVAL, trackball_settings_apply(&current, &next, &adapter));
    CHECK(record_equal(&before, &current), "an invalid current record must not mutate state");
    CHECK_INT(0, check_no_writes());
    return 0;
}

static int test_rejects_an_encoder_or_unbound_position_without_writes(void) {
    struct trackball_settings_record current = disabled_record(6);
    struct trackball_settings_request next = request(800, 200, true, 11, 6);
    struct trackball_settings_record before = current;

    fake_reset(&current);

    CHECK_INT(-EINVAL, trackball_settings_apply(&current, &next, &adapter));
    CHECK(record_equal(&before, &current), "an unbound position must not mutate the record");
    CHECK_INT(0, check_no_writes());
    return 0;
}

static void prepare_position_change(struct trackball_settings_record *current,
                                    struct trackball_settings_request *next,
                                    struct zmk_behavior_binding *former_wrapper,
                                    struct zmk_behavior_binding *new_original) {
    *current = (struct trackball_settings_record){
        .schema_version = TRACKBALL_SETTINGS_SCHEMA_VERSION,
        .enabled = true,
        .selected_position = 1,
        .normal_cpi = 800,
        .precision_cpi = 200,
        .original_behavior_id = TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_KP,
        .original_param1 = KEY_A,
        .original_param2 = 0,
        .revision = 21,
    };
    *next = request(1200, 400, true, 2, 21);
    *former_wrapper = binding("lt", TRACKBALL_SETTINGS_PRECISION_LAYER, KEY_A);
    *new_original = binding("kp", KEY_B, 0);
    fake_reset(current);
    fake_put_binding(1, *former_wrapper);
    fake_put_binding(2, *new_original);
}

static int check_rollback(const struct trackball_settings_record *before,
                          const struct zmk_behavior_binding *former_wrapper,
                          const struct zmk_behavior_binding *new_original) {
    CHECK(record_equal(before, &fake.persisted), "rollback must persist the prior record");
    CHECK(binding_equal(fake_get_binding(0, 1), former_wrapper),
          "rollback must restore the prior RAM wrapper");
    CHECK(binding_equal(fake_get_binding(0, 2), new_original),
          "rollback must restore the newly selected RAM binding");
    CHECK_INT((int)before->normal_cpi, fake.profile.normal_cpi);
    CHECK_INT((int)before->precision_cpi, fake.profile.precision_cpi);
    return 0;
}

static int test_rolls_back_after_set_binding_failure_and_returns_that_error(void) {
    struct trackball_settings_record current;
    struct trackball_settings_request next;
    struct trackball_settings_record before;
    struct zmk_behavior_binding former_wrapper;
    struct zmk_behavior_binding new_original;

    prepare_position_change(&current, &next, &former_wrapper, &new_original);
    before = current;
    fake.fail_set_call = 2;
    fake.set_error = -EACCES;

    CHECK_INT(-EACCES, trackball_settings_apply(&current, &next, &adapter));
    CHECK(record_equal(&before, &current), "a failed transaction must keep its RAM record");
    CHECK_INT(0, check_rollback(&before, &former_wrapper, &new_original));
    CHECK(fake.save_settings_calls >= 1, "rollback must persist the prior record after a set failure");
    return 0;
}

static int test_rolls_back_after_profile_failure_and_returns_that_error(void) {
    struct trackball_settings_record current;
    struct trackball_settings_request next;
    struct trackball_settings_record before;
    struct zmk_behavior_binding former_wrapper;
    struct zmk_behavior_binding new_original;

    prepare_position_change(&current, &next, &former_wrapper, &new_original);
    before = current;
    fake.fail_apply_profile_call = 1;
    fake.apply_profile_error = -EAGAIN;

    CHECK_INT(-EAGAIN, trackball_settings_apply(&current, &next, &adapter));
    CHECK(record_equal(&before, &current), "a failed transaction must keep its RAM record");
    CHECK_INT(0, check_rollback(&before, &former_wrapper, &new_original));
    CHECK(fake.save_settings_calls >= 1,
          "rollback must persist the prior record after a profile failure");
    return 0;
}

static int test_rolls_back_after_keymap_save_failure_and_returns_that_error(void) {
    struct trackball_settings_record current;
    struct trackball_settings_request next;
    struct trackball_settings_record before;
    struct zmk_behavior_binding former_wrapper;
    struct zmk_behavior_binding new_original;

    prepare_position_change(&current, &next, &former_wrapper, &new_original);
    before = current;
    fake.fail_save_keymap_call = 1;
    fake.save_keymap_error = -EBUSY;

    CHECK_INT(-EBUSY, trackball_settings_apply(&current, &next, &adapter));
    CHECK(record_equal(&before, &current), "a failed transaction must keep its RAM record");
    CHECK_INT(0, check_rollback(&before, &former_wrapper, &new_original));
    CHECK(fake.save_settings_calls >= 1,
          "rollback must persist the prior record after a keymap-save failure");
    return 0;
}

static int test_rolls_back_after_settings_save_failure_and_returns_that_error(void) {
    struct trackball_settings_record current;
    struct trackball_settings_request next;
    struct trackball_settings_record before;
    struct zmk_behavior_binding former_wrapper;
    struct zmk_behavior_binding new_original;

    prepare_position_change(&current, &next, &former_wrapper, &new_original);
    before = current;
    fake.fail_save_settings_call = 1;
    fake.save_settings_error = -ENOSPC;

    CHECK_INT(-ENOSPC, trackball_settings_apply(&current, &next, &adapter));
    CHECK(record_equal(&before, &current), "a failed transaction must keep its RAM record");
    CHECK_INT(0, check_rollback(&before, &former_wrapper, &new_original));
    CHECK(fake.save_settings_calls >= 2,
          "a settings-save failure must be compensated by saving the prior record");
    return 0;
}

static int test_reload_validates_schema_and_reapplies_wrapper_and_profile(void) {
    struct trackball_settings_record current = disabled_record(0);
    struct trackball_settings_request next = request(1200, 400, true, 6, 0);
    struct trackball_settings_record saved;
    struct zmk_behavior_binding original = binding("kp", KEY_C, 0);
    struct zmk_behavior_binding expected_wrapper = binding("lt", TRACKBALL_SETTINGS_PRECISION_LAYER, KEY_C);

    fake_reset(&current);
    fake_put_binding(6, original);
    CHECK_INT(0, trackball_settings_apply(&current, &next, &adapter));
    saved = current;

    fake_reset(&saved);
    fake_put_binding(6, original);
    CHECK_INT(0, trackball_settings_reload(&saved, &adapter));
    CHECK(binding_equal(fake_get_binding(0, 6), &expected_wrapper),
          "reload must restore the configured wrapper");
    CHECK_INT(1200, fake.profile.normal_cpi);
    CHECK_INT(400, fake.profile.precision_cpi);
    CHECK_INT(0, fake.save_keymap_calls);
    CHECK_INT(0, fake.save_settings_calls);

    saved.schema_version = TRACKBALL_SETTINGS_SCHEMA_VERSION + 1;
    fake_reset(&current);
    fake_put_binding(6, original);
    CHECK_INT(-EINVAL, trackball_settings_reload(&saved, &adapter));
    CHECK_INT(0, check_no_writes());
    return 0;
}

static int test_success_increments_revision_exactly_once(void) {
    struct trackball_settings_record current = disabled_record(41);
    struct trackball_settings_request enable = request(800, 200, true, 0, 41);
    struct trackball_settings_request update = request(1200, 400, true, 0, 42);

    fake_reset(&current);
    fake_put_binding(0, binding("kp", KEY_A, 0));

    CHECK_INT(0, trackball_settings_apply(&current, &enable, &adapter));
    CHECK_INT(42, (int)current.revision);
    CHECK_INT(0, trackball_settings_apply(&current, &update, &adapter));
    CHECK_INT(43, (int)current.revision);
    return 0;
}

#ifdef TRACKBALL_SETTINGS_HOST_TEST

int main(void) {
    return test_enables_kp_and_stores_exact_original_binding() ||
           test_enables_lt_and_preserves_tap_param2() ||
           test_enables_mt_and_preserves_tap_param2() ||
           test_enables_lt_mkp_and_preserves_mouse_tap() ||
           test_changing_positions_restores_previous_binding_before_wrapping_new_one() ||
           test_disabling_restores_original_binding_and_normal_profile() ||
           test_rejects_unsupported_and_precision_layer_bindings_without_writes() ||
           test_rejects_stale_revision_without_writes() ||
           test_rejects_an_invalid_current_record_without_writes() ||
           test_rejects_an_encoder_or_unbound_position_without_writes() ||
           test_rolls_back_after_set_binding_failure_and_returns_that_error() ||
           test_rolls_back_after_profile_failure_and_returns_that_error() ||
           test_rolls_back_after_keymap_save_failure_and_returns_that_error() ||
           test_rolls_back_after_settings_save_failure_and_returns_that_error() ||
           test_reload_validates_schema_and_reapplies_wrapper_and_profile() ||
           test_success_increments_revision_exactly_once();
}

#else

ZTEST(trackball_settings, test_enables_kp_and_stores_exact_original_binding) {
    zassert_equal(test_enables_kp_and_stores_exact_original_binding(), 0, "test failed");
}

ZTEST(trackball_settings, test_enables_lt_and_preserves_tap_param2) {
    zassert_equal(test_enables_lt_and_preserves_tap_param2(), 0, "test failed");
}

ZTEST(trackball_settings, test_enables_mt_and_preserves_tap_param2) {
    zassert_equal(test_enables_mt_and_preserves_tap_param2(), 0, "test failed");
}

ZTEST(trackball_settings, test_enables_lt_mkp_and_preserves_mouse_tap) {
    zassert_equal(test_enables_lt_mkp_and_preserves_mouse_tap(), 0, "test failed");
}

ZTEST(trackball_settings, test_changing_positions_restores_previous_binding_before_wrapping_new_one) {
    zassert_equal(test_changing_positions_restores_previous_binding_before_wrapping_new_one(), 0,
                  "test failed");
}

ZTEST(trackball_settings, test_disabling_restores_original_binding_and_normal_profile) {
    zassert_equal(test_disabling_restores_original_binding_and_normal_profile(), 0, "test failed");
}

ZTEST(trackball_settings, test_rejects_unsupported_and_precision_layer_bindings_without_writes) {
    zassert_equal(test_rejects_unsupported_and_precision_layer_bindings_without_writes(), 0,
                  "test failed");
}

ZTEST(trackball_settings, test_rejects_stale_revision_without_writes) {
    zassert_equal(test_rejects_stale_revision_without_writes(), 0, "test failed");
}

ZTEST(trackball_settings, test_rejects_an_invalid_current_record_without_writes) {
    zassert_equal(test_rejects_an_invalid_current_record_without_writes(), 0, "test failed");
}

ZTEST(trackball_settings, test_rejects_an_encoder_or_unbound_position_without_writes) {
    zassert_equal(test_rejects_an_encoder_or_unbound_position_without_writes(), 0, "test failed");
}

ZTEST(trackball_settings, test_rolls_back_after_set_binding_failure_and_returns_that_error) {
    zassert_equal(test_rolls_back_after_set_binding_failure_and_returns_that_error(), 0, "test failed");
}

ZTEST(trackball_settings, test_rolls_back_after_profile_failure_and_returns_that_error) {
    zassert_equal(test_rolls_back_after_profile_failure_and_returns_that_error(), 0, "test failed");
}

ZTEST(trackball_settings, test_rolls_back_after_keymap_save_failure_and_returns_that_error) {
    zassert_equal(test_rolls_back_after_keymap_save_failure_and_returns_that_error(), 0, "test failed");
}

ZTEST(trackball_settings, test_rolls_back_after_settings_save_failure_and_returns_that_error) {
    zassert_equal(test_rolls_back_after_settings_save_failure_and_returns_that_error(), 0, "test failed");
}

ZTEST(trackball_settings, test_reload_validates_schema_and_reapplies_wrapper_and_profile) {
    zassert_equal(test_reload_validates_schema_and_reapplies_wrapper_and_profile(), 0, "test failed");
}

ZTEST(trackball_settings, test_success_increments_revision_exactly_once) {
    zassert_equal(test_success_increments_revision_exactly_once(), 0, "test failed");
}

ZTEST_SUITE(trackball_settings, NULL, NULL, NULL, NULL, NULL);

#endif
