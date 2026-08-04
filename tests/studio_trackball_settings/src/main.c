#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef TRACKBALL_STUDIO_HOST_TEST
#include <stdio.h>
#else
#include <zephyr/ztest.h>
#endif

#include <pmw3610/trackball_settings.h>

/* These host/ZTEST seams exercise the same request-to-transaction mapping as the Studio handler. */
enum trackball_settings_rpc_test_request_kind {
    TRACKBALL_SETTINGS_RPC_TEST_GET = 1,
    TRACKBALL_SETTINGS_RPC_TEST_VALIDATE = 2,
    TRACKBALL_SETTINGS_RPC_TEST_APPLY = 3,
};

enum trackball_settings_rpc_test_result {
    TRACKBALL_SETTINGS_RPC_TEST_OK = 0,
    TRACKBALL_SETTINGS_RPC_TEST_INVALID_CPI = 1,
    TRACKBALL_SETTINGS_RPC_TEST_INVALID_POSITION = 2,
    TRACKBALL_SETTINGS_RPC_TEST_UNSUPPORTED_BINDING = 3,
    TRACKBALL_SETTINGS_RPC_TEST_STALE_REVISION = 4,
    TRACKBALL_SETTINGS_RPC_TEST_KEYMAP_WRITE_FAILED = 5,
    TRACKBALL_SETTINGS_RPC_TEST_SETTINGS_WRITE_FAILED = 6,
    TRACKBALL_SETTINGS_RPC_TEST_SENSOR_WRITE_FAILED = 7,
};

struct trackball_settings_rpc_test_binding {
    uint32_t behavior_id;
    uint32_t param1;
    uint32_t param2;
};

struct trackball_settings_rpc_test_config {
    uint32_t schema_version;
    uint32_t normal_cpi;
    uint32_t precision_cpi;
    bool enabled;
    uint32_t selected_position;
    struct trackball_settings_rpc_test_binding original_binding;
    uint32_t revision;
    bool precision_active;
    uint32_t current_cpi;
};

struct trackball_settings_rpc_test_response {
    enum trackball_settings_rpc_test_request_kind kind;
    enum trackball_settings_rpc_test_result result;
    struct trackball_settings_rpc_test_config config;
};

struct trackball_settings_rpc_test_context {
    struct trackball_settings_record record;
    const struct trackball_settings_adapter *adapter;
    int notification_count;
    struct trackball_settings_rpc_test_config last_notification;
    int (*set_precision_active)(bool active, void *user_data);
    int (*get_precision_snapshot)(struct pmw3610_precision_snapshot *snapshot, void *user_data);
    void *user_data;
};

int trackball_settings_rpc_test_encode_request(
    enum trackball_settings_rpc_test_request_kind kind,
    const struct trackball_settings_request *request, uint8_t *buffer, size_t capacity,
    size_t *encoded_size);
int trackball_settings_rpc_test_decode_request(
    const uint8_t *buffer, size_t size, enum trackball_settings_rpc_test_request_kind *kind,
    struct trackball_settings_request *request);
int trackball_settings_rpc_test_dispatch(
    struct trackball_settings_rpc_test_context *context,
    enum trackball_settings_rpc_test_request_kind kind,
    const struct trackball_settings_request *request,
    struct trackball_settings_rpc_test_response *response);
int trackball_settings_rpc_test_precision_layer_changed(
    struct trackball_settings_rpc_test_context *context, uint8_t layer, bool active);

#define TEST_POSITION_COUNT 12U
#define KEY_A 0x00070004U

struct fake_state {
    struct zmk_behavior_binding bindings[TEST_POSITION_COUNT];
    bool binding_present[TEST_POSITION_COUNT];
    struct trackball_profile profile;
    struct trackball_settings_record persisted;
    int set_binding_calls;
    int save_keymap_calls;
    int save_settings_calls;
    int apply_profile_calls;
    int set_precision_active_calls;
    bool last_precision_active;
    int fail_set_binding_call;
    int fail_save_keymap_call;
    int fail_save_settings_call;
    int fail_apply_profile_call;
    int set_binding_error;
    int save_keymap_error;
    int save_settings_error;
    int apply_profile_error;
    bool runtime_precision_active;
    uint16_t runtime_current_cpi;
};

static struct fake_state fake;

static const struct zmk_behavior_binding *fake_get_binding(uint8_t layer, uint8_t position) {
    if (layer != TRACKBALL_SETTINGS_BASE_LAYER || position >= TEST_POSITION_COUNT ||
        !fake.binding_present[position]) {
        return NULL;
    }

    return &fake.bindings[position];
}

static int fake_set_binding(uint8_t layer, uint8_t position, struct zmk_behavior_binding binding) {
    fake.set_binding_calls++;
    if (fake.fail_set_binding_call == fake.set_binding_calls) {
        return fake.set_binding_error;
    }
    if (layer != TRACKBALL_SETTINGS_BASE_LAYER || position >= TEST_POSITION_COUNT) {
        return -EINVAL;
    }

    fake.bindings[position] = binding;
    fake.binding_present[position] = true;
    return 0;
}

static int fake_save_keymap(void) {
    fake.save_keymap_calls++;
    if (fake.fail_save_keymap_call == fake.save_keymap_calls) {
        return fake.save_keymap_error;
    }
    return 0;
}

static int fake_save_settings(const struct trackball_settings_record *record) {
    fake.save_settings_calls++;
    if (fake.fail_save_settings_call == fake.save_settings_calls) {
        return fake.save_settings_error;
    }
    fake.persisted = *record;
    return 0;
}

static int fake_apply_profile(const struct trackball_profile *profile) {
    fake.apply_profile_calls++;
    if (fake.fail_apply_profile_call == fake.apply_profile_calls) {
        return fake.apply_profile_error;
    }
    fake.profile = *profile;
    fake.runtime_current_cpi = trackball_profile_cpi(profile, fake.runtime_precision_active);
    return 0;
}

static int fake_set_precision_active(bool active, void *user_data) {
    struct fake_state *state = user_data;

    state->set_precision_active_calls++;
    state->last_precision_active = active;
    state->runtime_precision_active = active;
    state->runtime_current_cpi = trackball_profile_cpi(&state->profile, active);
    return 0;
}

static int fake_report_data_set_precision_active(bool active) {
    return fake_set_precision_active(active, &fake);
}

static int fake_get_precision_snapshot(struct pmw3610_precision_snapshot *snapshot,
                                       void *user_data) {
    const struct fake_state *state = user_data;

    if (snapshot == NULL || state == NULL) {
        return -EINVAL;
    }
    *snapshot = (struct pmw3610_precision_snapshot){
        .precision_active = state->runtime_precision_active,
        .current_cpi = state->runtime_current_cpi,
    };
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
        .revision = revision,
    };
}

static struct trackball_settings_request apply_request(uint16_t normal_cpi, uint16_t precision_cpi,
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

static void fake_reset(const struct trackball_settings_record *record, const char *behavior) {
    memset(&fake, 0, sizeof(fake));
    fake.profile = (struct trackball_profile){
        .normal_cpi = record->normal_cpi,
        .precision_cpi = record->precision_cpi,
    };
    fake.persisted = *record;
    fake.set_binding_error = -EIO;
    fake.save_keymap_error = -EAGAIN;
    fake.save_settings_error = -EIO;
    fake.apply_profile_error = -EIO;
    fake.runtime_precision_active = false;
    fake.runtime_current_cpi = record->normal_cpi;
    if (behavior != NULL) {
        fake.bindings[5] = (struct zmk_behavior_binding){
            .behavior_dev = behavior,
            .param1 = KEY_A,
            .param2 = 0,
        };
        fake.binding_present[5] = true;
    }
}

static struct trackball_settings_rpc_test_context test_context(
    const struct trackball_settings_record *record) {
    return (struct trackball_settings_rpc_test_context){
        .record = *record,
        .adapter = &adapter,
        .set_precision_active = fake_set_precision_active,
        .get_precision_snapshot = fake_get_precision_snapshot,
        .user_data = &fake,
    };
}

#ifdef TRACKBALL_STUDIO_HOST_TEST

#define CHECK(condition, message)                                                                 \
    do {                                                                                          \
        if (!(condition)) {                                                                       \
            fprintf(stderr, "%s:%d: %s\\n", __func__, __LINE__, message);                    \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

#define CHECK_EQ(expected, actual)                                                               \
    do {                                                                                          \
        uint32_t actual_value = (uint32_t)(actual);                                              \
        if (actual_value != (uint32_t)(expected)) {                                              \
            fprintf(stderr, "%s:%d: expected %u, got %u\\n", __func__, __LINE__,             \
                    (unsigned int)(expected), (unsigned int)actual_value);                      \
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

#define CHECK_EQ(expected, actual)                                                               \
    do {                                                                                          \
        if ((uint32_t)(actual) != (uint32_t)(expected)) {                                        \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

#endif

static int test_locked_request_fixtures_encode_and_decode(void) {
    static const uint8_t get_fixture[] = {0x0A, 0x00};
    static const uint8_t enabled_fixture[] = {
        0x1A, 0x0C, 0x08, 0xA0, 0x06, 0x10, 0xC8, 0x01,
        0x18, 0x01, 0x20, 0x05, 0x28, 0x07,
    };
    static const uint8_t disabled_fixture[] = {
        0x1A, 0x08, 0x08, 0xA0, 0x06, 0x10, 0xC8, 0x01, 0x28, 0x07,
    };
    struct trackball_settings_request expected_enabled = apply_request(800, 200, true, 5, 7);
    struct trackball_settings_request expected_disabled = apply_request(800, 200, false, 0, 7);
    struct trackball_settings_request decoded = {0};
    enum trackball_settings_rpc_test_request_kind kind = 0;
    uint8_t encoded[32] = {0};
    size_t encoded_size = 0;

    CHECK_EQ(0, trackball_settings_rpc_test_encode_request(TRACKBALL_SETTINGS_RPC_TEST_GET, NULL,
                                                            encoded, sizeof(encoded), &encoded_size));
    CHECK_EQ(sizeof(get_fixture), encoded_size);
    CHECK(memcmp(get_fixture, encoded, sizeof(get_fixture)) == 0, "get request fixture changed");
    CHECK_EQ(0, trackball_settings_rpc_test_decode_request(get_fixture, sizeof(get_fixture), &kind,
                                                            &decoded));
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_GET, kind);

    CHECK_EQ(0, trackball_settings_rpc_test_encode_request(TRACKBALL_SETTINGS_RPC_TEST_APPLY,
                                                            &expected_enabled, encoded,
                                                            sizeof(encoded), &encoded_size));
    CHECK_EQ(sizeof(enabled_fixture), encoded_size);
    CHECK(memcmp(enabled_fixture, encoded, sizeof(enabled_fixture)) == 0,
          "enabled apply request fixture changed");
    CHECK_EQ(0, trackball_settings_rpc_test_decode_request(enabled_fixture, sizeof(enabled_fixture),
                                                            &kind, &decoded));
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_APPLY, kind);
    CHECK_EQ(800, decoded.normal_cpi);
    CHECK_EQ(200, decoded.precision_cpi);
    CHECK(decoded.enabled, "enabled request must preserve its boolean field");
    CHECK_EQ(5, decoded.selected_position);
    CHECK_EQ(7, decoded.expected_revision);

    CHECK_EQ(0, trackball_settings_rpc_test_encode_request(TRACKBALL_SETTINGS_RPC_TEST_APPLY,
                                                            &expected_disabled, encoded,
                                                            sizeof(encoded), &encoded_size));
    CHECK_EQ(sizeof(disabled_fixture), encoded_size);
    CHECK(memcmp(disabled_fixture, encoded, sizeof(disabled_fixture)) == 0,
          "disabled apply request fixture changed");
    CHECK_EQ(0, trackball_settings_rpc_test_decode_request(disabled_fixture,
                                                            sizeof(disabled_fixture), &kind, &decoded));
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_APPLY, kind);
    CHECK(!decoded.enabled, "disabled request must omit and decode false fields");
    CHECK_EQ(0, decoded.selected_position);
    CHECK_EQ(7, decoded.expected_revision);
    return 0;
}

static int test_get_reads_authoritative_config_without_writes(void) {
    struct trackball_settings_record record = disabled_record(7);
    struct trackball_settings_rpc_test_context context;
    struct trackball_settings_rpc_test_response response = {0};

    fake_reset(&record, "kp");
    context = test_context(&record);

    CHECK_EQ(0, trackball_settings_rpc_test_dispatch(&context, TRACKBALL_SETTINGS_RPC_TEST_GET, NULL,
                                                      &response));
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_GET, response.kind);
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_OK, response.result);
    CHECK_EQ(TRACKBALL_SETTINGS_SCHEMA_VERSION, response.config.schema_version);
    CHECK_EQ(800, response.config.normal_cpi);
    CHECK_EQ(200, response.config.precision_cpi);
    CHECK(!response.config.enabled, "default record must remain disabled");
    CHECK_EQ(7, response.config.revision);
    CHECK(!response.config.precision_active, "get must report inactive precision mode");
    CHECK_EQ(800, response.config.current_cpi);
    CHECK_EQ(0, fake.set_binding_calls);
    CHECK_EQ(0, fake.apply_profile_calls);
    CHECK_EQ(0, fake.save_keymap_calls);
    CHECK_EQ(0, fake.save_settings_calls);
    return 0;
}

static int test_validate_checks_without_writing(void) {
    struct trackball_settings_record record = disabled_record(7);
    struct trackball_settings_request request = apply_request(800, 200, true, 5, 7);
    struct trackball_settings_rpc_test_context context;
    struct trackball_settings_rpc_test_response response = {0};

    fake_reset(&record, "kp");
    context = test_context(&record);

    CHECK_EQ(0, trackball_settings_rpc_test_dispatch(&context,
                                                      TRACKBALL_SETTINGS_RPC_TEST_VALIDATE, &request,
                                                      &response));
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_VALIDATE, response.kind);
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_OK, response.result);
    CHECK_EQ(7, response.config.revision);
    CHECK(!response.config.enabled, "validate must return unchanged authoritative readback");
    CHECK_EQ(0, fake.set_binding_calls);
    CHECK_EQ(0, fake.apply_profile_calls);
    CHECK_EQ(0, fake.save_keymap_calls);
    CHECK_EQ(0, fake.save_settings_calls);
    CHECK_EQ(0, context.notification_count);
    return 0;
}

static int test_apply_returns_persisted_readback_and_notifies(void) {
    struct trackball_settings_record record = disabled_record(7);
    struct trackball_settings_request request = apply_request(800, 200, true, 5, 7);
    struct trackball_settings_rpc_test_context context;
    struct trackball_settings_rpc_test_response response = {0};

    fake_reset(&record, "kp");
    context = test_context(&record);
    CHECK_EQ(0, fake_report_data_set_precision_active(true));

    CHECK_EQ(0, trackball_settings_rpc_test_dispatch(&context, TRACKBALL_SETTINGS_RPC_TEST_APPLY,
                                                      &request, &response));
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_APPLY, response.kind);
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_OK, response.result);
    CHECK(response.config.enabled, "successful apply must read back enabled state");
    CHECK_EQ(5, response.config.selected_position);
    CHECK_EQ(TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_KP, response.config.original_binding.behavior_id);
    CHECK_EQ(KEY_A, response.config.original_binding.param1);
    CHECK_EQ(8, response.config.revision);
    CHECK(response.config.precision_active,
          "apply must read the current precision state from the runtime snapshot");
    CHECK_EQ(200, response.config.current_cpi);
    CHECK_EQ(1, fake.set_binding_calls);
    CHECK_EQ(1, fake.apply_profile_calls);
    CHECK_EQ(1, fake.save_keymap_calls);
    CHECK_EQ(1, fake.save_settings_calls);
    CHECK_EQ(1, context.notification_count);
    CHECK(context.last_notification.enabled, "success notification must contain committed config");
    CHECK_EQ(8, context.last_notification.revision);
    CHECK(context.last_notification.precision_active,
          "the success notification must use the runtime precision snapshot");
    CHECK_EQ(200, context.last_notification.current_cpi);
    return 0;
}

static int test_apply_maps_sensor_failure_stage_independent_of_errno(void) {
    struct trackball_settings_record record = disabled_record(7);
    struct trackball_settings_request request = apply_request(800, 200, true, 5, 7);
    struct trackball_settings_rpc_test_context context;
    struct trackball_settings_rpc_test_response response = {0};

    fake_reset(&record, "kp");
    fake.fail_apply_profile_call = 1;
    fake.apply_profile_error = -EIO;
    context = test_context(&record);

    CHECK_EQ(0, trackball_settings_rpc_test_dispatch(&context, TRACKBALL_SETTINGS_RPC_TEST_APPLY,
                                                      &request, &response));
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_SENSOR_WRITE_FAILED, response.result);
    CHECK_EQ(7, response.config.revision);
    CHECK_EQ(0, context.notification_count);
    return 0;
}

static int test_apply_maps_keymap_failure_stage_independent_of_errno(void) {
    struct trackball_settings_record record = disabled_record(7);
    struct trackball_settings_request request = apply_request(800, 200, true, 5, 7);
    struct trackball_settings_rpc_test_context context;
    struct trackball_settings_rpc_test_response response = {0};

    fake_reset(&record, "kp");
    fake.fail_save_keymap_call = 1;
    fake.save_keymap_error = -EAGAIN;
    context = test_context(&record);

    CHECK_EQ(0, trackball_settings_rpc_test_dispatch(&context, TRACKBALL_SETTINGS_RPC_TEST_APPLY,
                                                      &request, &response));
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_KEYMAP_WRITE_FAILED, response.result);
    CHECK_EQ(7, response.config.revision);
    CHECK_EQ(0, context.notification_count);
    return 0;
}

static int test_apply_maps_settings_failure_stage_independent_of_errno(void) {
    struct trackball_settings_record record = disabled_record(7);
    struct trackball_settings_request request = apply_request(800, 200, true, 5, 7);
    struct trackball_settings_rpc_test_context context;
    struct trackball_settings_rpc_test_response response = {0};

    fake_reset(&record, "kp");
    fake.fail_save_settings_call = 1;
    fake.save_settings_error = -EIO;
    context = test_context(&record);

    CHECK_EQ(0, trackball_settings_rpc_test_dispatch(&context, TRACKBALL_SETTINGS_RPC_TEST_APPLY,
                                                      &request, &response));
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_SETTINGS_WRITE_FAILED, response.result);
    CHECK_EQ(7, response.config.revision);
    CHECK_EQ(0, context.notification_count);
    return 0;
}

static int test_get_observes_precision_transition_outside_the_handler(void) {
    struct trackball_settings_record record = disabled_record(7);
    struct trackball_settings_rpc_test_context context;
    struct trackball_settings_rpc_test_response response = {0};

    fake_reset(&record, "kp");
    context = test_context(&record);

    CHECK_EQ(0, fake_report_data_set_precision_active(true));
    CHECK_EQ(0, trackball_settings_rpc_test_dispatch(&context, TRACKBALL_SETTINGS_RPC_TEST_GET, NULL,
                                                      &response));
    CHECK(response.config.precision_active,
          "get must use the runtime snapshot after report-data changes precision mode");
    CHECK_EQ(200, response.config.current_cpi);
    return 0;
}

static int test_stale_revision_returns_readback_without_writes(void) {
    struct trackball_settings_record record = disabled_record(7);
    struct trackball_settings_request request = apply_request(800, 200, true, 5, 6);
    struct trackball_settings_rpc_test_context context;
    struct trackball_settings_rpc_test_response response = {0};

    fake_reset(&record, "kp");
    context = test_context(&record);

    CHECK_EQ(0, trackball_settings_rpc_test_dispatch(&context, TRACKBALL_SETTINGS_RPC_TEST_APPLY,
                                                      &request, &response));
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_STALE_REVISION, response.result);
    CHECK_EQ(7, response.config.revision);
    CHECK_EQ(0, fake.set_binding_calls);
    CHECK_EQ(0, fake.apply_profile_calls);
    CHECK_EQ(0, fake.save_keymap_calls);
    CHECK_EQ(0, fake.save_settings_calls);
    CHECK_EQ(0, context.notification_count);
    return 0;
}

static int test_invalid_cpi_returns_error_without_writes(void) {
    struct trackball_settings_record record = disabled_record(7);
    struct trackball_settings_request request = apply_request(700, 200, true, 5, 7);
    struct trackball_settings_rpc_test_context context;
    struct trackball_settings_rpc_test_response response = {0};

    fake_reset(&record, "kp");
    context = test_context(&record);

    CHECK_EQ(0, trackball_settings_rpc_test_dispatch(&context, TRACKBALL_SETTINGS_RPC_TEST_APPLY,
                                                      &request, &response));
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_INVALID_CPI, response.result);
    CHECK_EQ(7, response.config.revision);
    CHECK_EQ(0, fake.set_binding_calls);
    CHECK_EQ(0, fake.apply_profile_calls);
    CHECK_EQ(0, fake.save_keymap_calls);
    CHECK_EQ(0, fake.save_settings_calls);
    return 0;
}

static int test_unsupported_binding_returns_error_without_writes(void) {
    struct trackball_settings_record record = disabled_record(7);
    struct trackball_settings_request request = apply_request(800, 200, true, 5, 7);
    struct trackball_settings_rpc_test_context context;
    struct trackball_settings_rpc_test_response response = {0};

    fake_reset(&record, "unsupported");
    context = test_context(&record);

    CHECK_EQ(0, trackball_settings_rpc_test_dispatch(&context,
                                                      TRACKBALL_SETTINGS_RPC_TEST_VALIDATE, &request,
                                                      &response));
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_UNSUPPORTED_BINDING, response.result);
    CHECK_EQ(7, response.config.revision);
    CHECK_EQ(0, fake.set_binding_calls);
    CHECK_EQ(0, fake.apply_profile_calls);
    CHECK_EQ(0, fake.save_keymap_calls);
    CHECK_EQ(0, fake.save_settings_calls);
    return 0;
}

static int test_unsupported_position_returns_error_without_writes(void) {
    struct trackball_settings_record record = disabled_record(7);
    struct trackball_settings_request request = apply_request(800, 200, true, 11, 7);
    struct trackball_settings_rpc_test_context context;
    struct trackball_settings_rpc_test_response response = {0};

    fake_reset(&record, "kp");
    context = test_context(&record);

    CHECK_EQ(0, trackball_settings_rpc_test_dispatch(&context,
                                                      TRACKBALL_SETTINGS_RPC_TEST_VALIDATE, &request,
                                                      &response));
    CHECK_EQ(TRACKBALL_SETTINGS_RPC_TEST_INVALID_POSITION, response.result);
    CHECK_EQ(7, response.config.revision);
    CHECK_EQ(0, fake.set_binding_calls);
    CHECK_EQ(0, fake.apply_profile_calls);
    CHECK_EQ(0, fake.save_keymap_calls);
    CHECK_EQ(0, fake.save_settings_calls);
    CHECK_EQ(0, context.notification_count);
    return 0;
}

static int test_precision_layer_changes_apply_immediately_and_notify(void) {
    struct trackball_settings_record record = disabled_record(8);
    struct trackball_settings_rpc_test_context context;

    fake_reset(&record, "kp");
    context = test_context(&record);

    CHECK_EQ(0, trackball_settings_rpc_test_precision_layer_changed(
                    &context, TRACKBALL_SETTINGS_PRECISION_LAYER, true));
    CHECK_EQ(1, fake.set_precision_active_calls);
    CHECK(fake.last_precision_active, "the press transition must enable precision immediately");
    CHECK_EQ(1, context.notification_count);
    CHECK(context.last_notification.precision_active,
          "the press notification must be authoritative about active precision");
    CHECK_EQ(200, context.last_notification.current_cpi);

    CHECK_EQ(0, trackball_settings_rpc_test_precision_layer_changed(
                    &context, TRACKBALL_SETTINGS_PRECISION_LAYER, false));
    CHECK_EQ(2, fake.set_precision_active_calls);
    CHECK(!fake.last_precision_active, "the release transition must restore normal CPI immediately");
    CHECK_EQ(2, context.notification_count);
    CHECK(!context.last_notification.precision_active,
          "the release notification must be authoritative about inactive precision");
    CHECK_EQ(800, context.last_notification.current_cpi);
    return 0;
}

#ifdef TRACKBALL_STUDIO_HOST_TEST

int main(void) {
    int (*const tests[])(void) = {
        test_locked_request_fixtures_encode_and_decode,
        test_get_reads_authoritative_config_without_writes,
        test_validate_checks_without_writing,
        test_apply_returns_persisted_readback_and_notifies,
        test_apply_maps_sensor_failure_stage_independent_of_errno,
        test_apply_maps_keymap_failure_stage_independent_of_errno,
        test_apply_maps_settings_failure_stage_independent_of_errno,
        test_get_observes_precision_transition_outside_the_handler,
        test_stale_revision_returns_readback_without_writes,
        test_invalid_cpi_returns_error_without_writes,
        test_unsupported_binding_returns_error_without_writes,
        test_unsupported_position_returns_error_without_writes,
        test_precision_layer_changes_apply_immediately_and_notify,
    };

    for (size_t index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
        if (tests[index]() != 0) {
            return 1;
        }
    }
    return 0;
}

#else

ZTEST_SUITE(studio_trackball_settings, NULL, NULL, NULL, NULL, NULL);

ZTEST(studio_trackball_settings, test_locked_request_fixtures_encode_and_decode) {
    zassert_equal(0, test_locked_request_fixtures_encode_and_decode());
}

ZTEST(studio_trackball_settings, test_get_reads_authoritative_config_without_writes) {
    zassert_equal(0, test_get_reads_authoritative_config_without_writes());
}

ZTEST(studio_trackball_settings, test_validate_checks_without_writing) {
    zassert_equal(0, test_validate_checks_without_writing());
}

ZTEST(studio_trackball_settings, test_apply_returns_persisted_readback_and_notifies) {
    zassert_equal(0, test_apply_returns_persisted_readback_and_notifies());
}

ZTEST(studio_trackball_settings, test_apply_maps_sensor_failure_stage_independent_of_errno) {
    zassert_equal(0, test_apply_maps_sensor_failure_stage_independent_of_errno());
}

ZTEST(studio_trackball_settings, test_apply_maps_keymap_failure_stage_independent_of_errno) {
    zassert_equal(0, test_apply_maps_keymap_failure_stage_independent_of_errno());
}

ZTEST(studio_trackball_settings, test_apply_maps_settings_failure_stage_independent_of_errno) {
    zassert_equal(0, test_apply_maps_settings_failure_stage_independent_of_errno());
}

ZTEST(studio_trackball_settings, test_get_observes_precision_transition_outside_the_handler) {
    zassert_equal(0, test_get_observes_precision_transition_outside_the_handler());
}

ZTEST(studio_trackball_settings, test_stale_revision_returns_readback_without_writes) {
    zassert_equal(0, test_stale_revision_returns_readback_without_writes());
}

ZTEST(studio_trackball_settings, test_invalid_cpi_returns_error_without_writes) {
    zassert_equal(0, test_invalid_cpi_returns_error_without_writes());
}

ZTEST(studio_trackball_settings, test_unsupported_binding_returns_error_without_writes) {
    zassert_equal(0, test_unsupported_binding_returns_error_without_writes());
}

ZTEST(studio_trackball_settings, test_unsupported_position_returns_error_without_writes) {
    zassert_equal(0, test_unsupported_position_returns_error_without_writes());
}

ZTEST(studio_trackball_settings, test_precision_layer_changes_apply_immediately_and_notify) {
    zassert_equal(0, test_precision_layer_changes_apply_immediately_and_notify());
}

#endif
