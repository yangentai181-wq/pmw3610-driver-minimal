#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <pmw3610/trackball_settings.h>

enum trackball_settings_rpc_request_kind {
    TRACKBALL_SETTINGS_RPC_GET = 1,
    TRACKBALL_SETTINGS_RPC_VALIDATE = 2,
    TRACKBALL_SETTINGS_RPC_APPLY = 3,
};

enum trackball_settings_rpc_result {
    TRACKBALL_SETTINGS_RPC_OK = 0,
    TRACKBALL_SETTINGS_RPC_INVALID_CPI = 1,
    TRACKBALL_SETTINGS_RPC_INVALID_POSITION = 2,
    TRACKBALL_SETTINGS_RPC_UNSUPPORTED_BINDING = 3,
    TRACKBALL_SETTINGS_RPC_STALE_REVISION = 4,
    TRACKBALL_SETTINGS_RPC_KEYMAP_WRITE_FAILED = 5,
    TRACKBALL_SETTINGS_RPC_SETTINGS_WRITE_FAILED = 6,
    TRACKBALL_SETTINGS_RPC_SENSOR_WRITE_FAILED = 7,
};

struct trackball_settings_rpc_binding {
    uint32_t behavior_id;
    uint32_t param1;
    uint32_t param2;
};

struct trackball_settings_rpc_config {
    uint32_t schema_version;
    uint32_t normal_cpi;
    uint32_t precision_cpi;
    bool enabled;
    uint32_t selected_position;
    struct trackball_settings_rpc_binding original_binding;
    uint32_t revision;
    bool precision_active;
    uint32_t current_cpi;
};

struct trackball_settings_rpc_response {
    enum trackball_settings_rpc_request_kind kind;
    enum trackball_settings_rpc_result result;
    struct trackball_settings_rpc_config config;
};

struct trackball_settings_rpc_service {
    void *context;
    int (*get_record)(void *context, struct trackball_settings_record *record);
    int (*validate)(void *context, const struct trackball_settings_request *request);
    int (*apply)(void *context, const struct trackball_settings_request *request,
                 enum trackball_settings_failure_stage *failure_stage);
    int (*set_precision_active)(void *context, bool active);
    int (*get_precision_snapshot)(void *context, struct pmw3610_precision_snapshot *snapshot);
    void (*notify)(void *context, const struct trackball_settings_rpc_config *config);
};

static enum trackball_settings_rpc_result trackball_settings_rpc_result_from_error(
    int err, enum trackball_settings_failure_stage failure_stage) {
    if (err != 0) {
        switch (failure_stage) {
        case TRACKBALL_SETTINGS_FAILURE_STAGE_KEYMAP:
            return TRACKBALL_SETTINGS_RPC_KEYMAP_WRITE_FAILED;
        case TRACKBALL_SETTINGS_FAILURE_STAGE_SETTINGS:
            return TRACKBALL_SETTINGS_RPC_SETTINGS_WRITE_FAILED;
        case TRACKBALL_SETTINGS_FAILURE_STAGE_SENSOR:
            return TRACKBALL_SETTINGS_RPC_SENSOR_WRITE_FAILED;
        case TRACKBALL_SETTINGS_FAILURE_STAGE_NONE:
            break;
        }
    }

    switch (err) {
    case 0:
        return TRACKBALL_SETTINGS_RPC_OK;
    case -ESTALE:
        return TRACKBALL_SETTINGS_RPC_STALE_REVISION;
    case -ENOTSUP:
        return TRACKBALL_SETTINGS_RPC_UNSUPPORTED_BINDING;
    case -EINVAL:
        return TRACKBALL_SETTINGS_RPC_INVALID_POSITION;
    default:
        return TRACKBALL_SETTINGS_RPC_SETTINGS_WRITE_FAILED;
    }
}

static int trackball_settings_rpc_fill_config(
    const struct trackball_settings_rpc_service *service,
    struct trackball_settings_rpc_config *config) {
    struct trackball_settings_record record;
    struct pmw3610_precision_snapshot precision_snapshot;
    int err;

    if (service == NULL || config == NULL || service->get_record == NULL ||
        service->get_precision_snapshot == NULL) {
        return -EINVAL;
    }

    err = service->get_record(service->context, &record);
    if (err != 0) {
        return err;
    }
    err = service->get_precision_snapshot(service->context, &precision_snapshot);
    if (err != 0) {
        return err;
    }

    *config = (struct trackball_settings_rpc_config){
        .schema_version = record.schema_version,
        .normal_cpi = record.normal_cpi,
        .precision_cpi = record.precision_cpi,
        .enabled = record.enabled,
        .selected_position = record.selected_position,
        .original_binding =
            {
                .behavior_id = record.original_behavior_id,
                .param1 = record.original_param1,
                .param2 = record.original_param2,
            },
        .revision = record.revision,
        .precision_active = precision_snapshot.precision_active,
        .current_cpi = precision_snapshot.current_cpi,
    };
    return 0;
}

static int trackball_settings_rpc_dispatch(
    const struct trackball_settings_rpc_service *service,
    enum trackball_settings_rpc_request_kind kind,
    const struct trackball_settings_request *request,
    struct trackball_settings_rpc_response *response) {
    enum trackball_settings_failure_stage failure_stage = TRACKBALL_SETTINGS_FAILURE_STAGE_NONE;
    int err = 0;
    int readback_err;

    if (service == NULL || response == NULL) {
        return -EINVAL;
    }

    *response = (struct trackball_settings_rpc_response){
        .kind = kind,
        .result = TRACKBALL_SETTINGS_RPC_SETTINGS_WRITE_FAILED,
    };

    if (kind == TRACKBALL_SETTINGS_RPC_GET) {
        err = trackball_settings_rpc_fill_config(service, &response->config);
        response->result = trackball_settings_rpc_result_from_error(err, failure_stage);
        return 0;
    }
    if ((kind != TRACKBALL_SETTINGS_RPC_VALIDATE && kind != TRACKBALL_SETTINGS_RPC_APPLY) ||
        request == NULL || service->validate == NULL || service->apply == NULL) {
        return -EINVAL;
    }

    if (trackball_profile_validate(request->normal_cpi, request->precision_cpi) != 0) {
        response->result = TRACKBALL_SETTINGS_RPC_INVALID_CPI;
    } else {
        err = service->validate(service->context, request);
        if (err == 0 && kind == TRACKBALL_SETTINGS_RPC_APPLY) {
            err = service->apply(service->context, request, &failure_stage);
        }
        response->result = trackball_settings_rpc_result_from_error(err, failure_stage);
    }

    readback_err = trackball_settings_rpc_fill_config(service, &response->config);
    if (response->result == TRACKBALL_SETTINGS_RPC_OK && readback_err != 0) {
        response->result = trackball_settings_rpc_result_from_error(
            readback_err, TRACKBALL_SETTINGS_FAILURE_STAGE_NONE);
    }
    if (kind == TRACKBALL_SETTINGS_RPC_APPLY && response->result == TRACKBALL_SETTINGS_RPC_OK &&
        service->notify != NULL) {
        service->notify(service->context, &response->config);
    }
    return 0;
}

#if !defined(TRACKBALL_STUDIO_HOST_TEST) && !defined(TRACKBALL_STUDIO_ZTEST)
static void trackball_settings_rpc_response_with_result(
    const struct trackball_settings_rpc_service *service,
    enum trackball_settings_rpc_request_kind kind,
    enum trackball_settings_rpc_result result,
    struct trackball_settings_rpc_response *response) {
    int err;

    *response = (struct trackball_settings_rpc_response){
        .kind = kind,
        .result = result,
    };
    err = trackball_settings_rpc_fill_config(service, &response->config);
    if (err != 0) {
        response->result = trackball_settings_rpc_result_from_error(
            err, TRACKBALL_SETTINGS_FAILURE_STAGE_NONE);
    }
}
#endif

static int trackball_settings_rpc_precision_layer_changed(
    const struct trackball_settings_rpc_service *service, uint8_t layer, bool active) {
    struct trackball_settings_rpc_config config;
    int err;

    if (service == NULL || layer != TRACKBALL_SETTINGS_PRECISION_LAYER) {
        return 0;
    }
    if (service->set_precision_active == NULL || service->notify == NULL) {
        return -EINVAL;
    }

    err = service->set_precision_active(service->context, active);
    if (err != 0) {
        return err;
    }
    err = trackball_settings_rpc_fill_config(service, &config);
    if (err != 0) {
        return err;
    }
    service->notify(service->context, &config);
    return 0;
}

#if defined(TRACKBALL_STUDIO_HOST_TEST) || defined(TRACKBALL_STUDIO_ZTEST)

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

static void trackball_settings_rpc_test_copy_config(
    struct trackball_settings_rpc_test_config *destination,
    const struct trackball_settings_rpc_config *source) {
    *destination = (struct trackball_settings_rpc_test_config){
        .schema_version = source->schema_version,
        .normal_cpi = source->normal_cpi,
        .precision_cpi = source->precision_cpi,
        .enabled = source->enabled,
        .selected_position = source->selected_position,
        .original_binding =
            {
                .behavior_id = source->original_binding.behavior_id,
                .param1 = source->original_binding.param1,
                .param2 = source->original_binding.param2,
            },
        .revision = source->revision,
        .precision_active = source->precision_active,
        .current_cpi = source->current_cpi,
    };
}

static int trackball_settings_rpc_test_get_record(void *context,
                                                  struct trackball_settings_record *record) {
    struct trackball_settings_rpc_test_context *test_context = context;

    if (test_context == NULL || record == NULL) {
        return -EINVAL;
    }
    *record = test_context->record;
    return 0;
}

static int trackball_settings_rpc_test_validate(void *context,
                                                const struct trackball_settings_request *request) {
    struct trackball_settings_rpc_test_context *test_context = context;

    if (test_context == NULL) {
        return -EINVAL;
    }
    return trackball_settings_validate(&test_context->record, request, test_context->adapter);
}

static int trackball_settings_rpc_test_apply(void *context,
                                             const struct trackball_settings_request *request,
                                             enum trackball_settings_failure_stage *failure_stage) {
    struct trackball_settings_rpc_test_context *test_context = context;

    if (test_context == NULL) {
        return -EINVAL;
    }
    return trackball_settings_apply_with_failure_stage(&test_context->record, request,
                                                       test_context->adapter, failure_stage);
}

static int trackball_settings_rpc_test_set_precision_active(void *context, bool active) {
    struct trackball_settings_rpc_test_context *test_context = context;

    if (test_context == NULL || test_context->set_precision_active == NULL) {
        return -EINVAL;
    }
    return test_context->set_precision_active(active, test_context->user_data);
}

static int trackball_settings_rpc_test_get_precision_snapshot(
    void *context, struct pmw3610_precision_snapshot *snapshot) {
    struct trackball_settings_rpc_test_context *test_context = context;

    if (test_context == NULL || test_context->get_precision_snapshot == NULL) {
        return -EINVAL;
    }
    return test_context->get_precision_snapshot(snapshot, test_context->user_data);
}

static void trackball_settings_rpc_test_notify(
    void *context, const struct trackball_settings_rpc_config *config) {
    struct trackball_settings_rpc_test_context *test_context = context;

    test_context->notification_count++;
    trackball_settings_rpc_test_copy_config(&test_context->last_notification, config);
}

static struct trackball_settings_rpc_service trackball_settings_rpc_test_service(
    struct trackball_settings_rpc_test_context *context) {
    return (struct trackball_settings_rpc_service){
        .context = context,
        .get_record = trackball_settings_rpc_test_get_record,
        .validate = trackball_settings_rpc_test_validate,
        .apply = trackball_settings_rpc_test_apply,
        .set_precision_active = trackball_settings_rpc_test_set_precision_active,
        .get_precision_snapshot = trackball_settings_rpc_test_get_precision_snapshot,
        .notify = trackball_settings_rpc_test_notify,
    };
}

static int trackball_settings_rpc_write_varint(uint32_t value, uint8_t *buffer, size_t capacity,
                                               size_t *offset) {
    do {
        if (*offset >= capacity) {
            return -ENOSPC;
        }
        buffer[*offset] = (uint8_t)(value & 0x7FU);
        value >>= 7U;
        if (value != 0U) {
            buffer[*offset] |= 0x80U;
        }
        (*offset)++;
    } while (value != 0U);
    return 0;
}

static int trackball_settings_rpc_read_varint(const uint8_t *buffer, size_t size, size_t *offset,
                                              uint32_t *value) {
    uint32_t result = 0;
    uint8_t shift = 0;

    while (*offset < size && shift < 32U) {
        const uint8_t byte = buffer[(*offset)++];

        result |= (uint32_t)(byte & 0x7FU) << shift;
        if ((byte & 0x80U) == 0U) {
            *value = result;
            return 0;
        }
        shift += 7U;
    }
    return -EINVAL;
}

static int trackball_settings_rpc_encode_apply_payload(
    const struct trackball_settings_request *request, uint8_t *buffer, size_t capacity,
    size_t *encoded_size) {
    size_t offset = 0;
    int err;

    if (request == NULL || buffer == NULL || encoded_size == NULL) {
        return -EINVAL;
    }
    if (request->normal_cpi != 0U) {
        err = trackball_settings_rpc_write_varint(0x08U, buffer, capacity, &offset);
        if (err != 0 || (err = trackball_settings_rpc_write_varint(request->normal_cpi, buffer,
                                                                     capacity, &offset)) != 0) {
            return err;
        }
    }
    if (request->precision_cpi != 0U) {
        err = trackball_settings_rpc_write_varint(0x10U, buffer, capacity, &offset);
        if (err != 0 || (err = trackball_settings_rpc_write_varint(request->precision_cpi, buffer,
                                                                     capacity, &offset)) != 0) {
            return err;
        }
    }
    if (request->enabled) {
        err = trackball_settings_rpc_write_varint(0x18U, buffer, capacity, &offset);
        if (err != 0 || (err = trackball_settings_rpc_write_varint(1U, buffer, capacity, &offset)) !=
                            0) {
            return err;
        }
    }
    if (request->selected_position != 0U) {
        err = trackball_settings_rpc_write_varint(0x20U, buffer, capacity, &offset);
        if (err != 0 ||
            (err = trackball_settings_rpc_write_varint(request->selected_position, buffer, capacity,
                                                        &offset)) != 0) {
            return err;
        }
    }
    if (request->expected_revision != 0U) {
        err = trackball_settings_rpc_write_varint(0x28U, buffer, capacity, &offset);
        if (err != 0 ||
            (err = trackball_settings_rpc_write_varint(request->expected_revision, buffer, capacity,
                                                        &offset)) != 0) {
            return err;
        }
    }
    *encoded_size = offset;
    return 0;
}

int trackball_settings_rpc_test_encode_request(
    enum trackball_settings_rpc_test_request_kind kind,
    const struct trackball_settings_request *request, uint8_t *buffer, size_t capacity,
    size_t *encoded_size) {
    uint8_t payload[24];
    size_t payload_size = 0;
    size_t offset = 0;
    uint8_t outer_tag;
    int err;

    if (buffer == NULL || encoded_size == NULL) {
        return -EINVAL;
    }
    if (kind == TRACKBALL_SETTINGS_RPC_TEST_GET) {
        if (capacity < 2U) {
            return -ENOSPC;
        }
        buffer[0] = 0x0AU;
        buffer[1] = 0x00U;
        *encoded_size = 2U;
        return 0;
    }
    if (kind != TRACKBALL_SETTINGS_RPC_TEST_VALIDATE && kind != TRACKBALL_SETTINGS_RPC_TEST_APPLY) {
        return -EINVAL;
    }
    err = trackball_settings_rpc_encode_apply_payload(request, payload, sizeof(payload), &payload_size);
    if (err != 0) {
        return err;
    }
    outer_tag = kind == TRACKBALL_SETTINGS_RPC_TEST_VALIDATE ? 0x12U : 0x1AU;
    err = trackball_settings_rpc_write_varint(outer_tag, buffer, capacity, &offset);
    if (err != 0 || (err = trackball_settings_rpc_write_varint((uint32_t)payload_size, buffer, capacity,
                                                                 &offset)) != 0) {
        return err;
    }
    if (capacity - offset < payload_size) {
        return -ENOSPC;
    }
    memcpy(buffer + offset, payload, payload_size);
    *encoded_size = offset + payload_size;
    return 0;
}

static int trackball_settings_rpc_decode_apply_payload(const uint8_t *buffer, size_t size,
                                                        struct trackball_settings_request *request) {
    size_t offset = 0;

    if (buffer == NULL || request == NULL) {
        return -EINVAL;
    }
    *request = (struct trackball_settings_request){0};
    while (offset < size) {
        uint32_t field;
        uint32_t value;

        if (trackball_settings_rpc_read_varint(buffer, size, &offset, &field) != 0 ||
            trackball_settings_rpc_read_varint(buffer, size, &offset, &value) != 0) {
            return -EINVAL;
        }
        switch (field) {
        case 0x08U:
            if (value > UINT16_MAX) {
                return -ERANGE;
            }
            request->normal_cpi = (uint16_t)value;
            break;
        case 0x10U:
            if (value > UINT16_MAX) {
                return -ERANGE;
            }
            request->precision_cpi = (uint16_t)value;
            break;
        case 0x18U:
            request->enabled = value != 0U;
            break;
        case 0x20U:
            if (value > UINT8_MAX) {
                return -ERANGE;
            }
            request->selected_position = (uint8_t)value;
            break;
        case 0x28U:
            request->expected_revision = value;
            break;
        default:
            return -EINVAL;
        }
    }
    return 0;
}

int trackball_settings_rpc_test_decode_request(
    const uint8_t *buffer, size_t size, enum trackball_settings_rpc_test_request_kind *kind,
    struct trackball_settings_request *request) {
    size_t offset = 0;
    uint32_t outer_tag;
    uint32_t payload_size;

    if (buffer == NULL || kind == NULL || request == NULL) {
        return -EINVAL;
    }
    if (trackball_settings_rpc_read_varint(buffer, size, &offset, &outer_tag) != 0 ||
        trackball_settings_rpc_read_varint(buffer, size, &offset, &payload_size) != 0 ||
        payload_size > size - offset || payload_size != size - offset) {
        return -EINVAL;
    }
    if (outer_tag == 0x0AU) {
        if (payload_size != 0U) {
            return -EINVAL;
        }
        *kind = TRACKBALL_SETTINGS_RPC_TEST_GET;
        *request = (struct trackball_settings_request){0};
        return 0;
    }
    if (outer_tag != 0x12U && outer_tag != 0x1AU) {
        return -EINVAL;
    }
    *kind = outer_tag == 0x12U ? TRACKBALL_SETTINGS_RPC_TEST_VALIDATE
                               : TRACKBALL_SETTINGS_RPC_TEST_APPLY;
    return trackball_settings_rpc_decode_apply_payload(buffer + offset, payload_size, request);
}

int trackball_settings_rpc_test_dispatch(
    struct trackball_settings_rpc_test_context *context,
    enum trackball_settings_rpc_test_request_kind kind,
    const struct trackball_settings_request *request,
    struct trackball_settings_rpc_test_response *response) {
    struct trackball_settings_rpc_response internal_response;
    struct trackball_settings_rpc_service service;
    int err;

    if (context == NULL || response == NULL) {
        return -EINVAL;
    }
    service = trackball_settings_rpc_test_service(context);
    err = trackball_settings_rpc_dispatch(
        &service, (enum trackball_settings_rpc_request_kind)kind, request, &internal_response);
    if (err != 0) {
        return err;
    }
    *response = (struct trackball_settings_rpc_test_response){
        .kind = (enum trackball_settings_rpc_test_request_kind)internal_response.kind,
        .result = (enum trackball_settings_rpc_test_result)internal_response.result,
    };
    trackball_settings_rpc_test_copy_config(&response->config, &internal_response.config);
    return 0;
}

int trackball_settings_rpc_test_precision_layer_changed(
    struct trackball_settings_rpc_test_context *context, uint8_t layer, bool active) {
    struct trackball_settings_rpc_service service;

    if (context == NULL) {
        return -EINVAL;
    }
    service = trackball_settings_rpc_test_service(context);
    return trackball_settings_rpc_precision_layer_changed(&service, layer, active);
}

#else

#include <pb_decode.h>
#include <pb_encode.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/iterable_sections.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/studio/custom.h>

#include <proto/zmk/trackball_settings/trackball_settings.pb.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static int trackball_settings_rpc_production_get_record(
    void *context, struct trackball_settings_record *record) {
    (void)context;
    return trackball_settings_get_record(record);
}

static int trackball_settings_rpc_production_validate(
    void *context, const struct trackball_settings_request *request) {
    (void)context;
    return trackball_settings_validate_request(request);
}

static int trackball_settings_rpc_production_apply(
    void *context, const struct trackball_settings_request *request,
    enum trackball_settings_failure_stage *failure_stage) {
    (void)context;
    return trackball_settings_apply_request_with_failure_stage(request, failure_stage);
}

static int trackball_settings_rpc_production_set_precision_active(void *context, bool active) {
    (void)context;
    return pmw3610_set_precision_active(active);
}

static int trackball_settings_rpc_production_get_precision_snapshot(
    void *context, struct pmw3610_precision_snapshot *snapshot) {
    (void)context;
    return pmw3610_get_precision_snapshot(snapshot);
}

static void trackball_settings_rpc_config_to_pb(
    zmk_trackball_settings_TrackballConfig *destination,
    const struct trackball_settings_rpc_config *source) {
    *destination = (zmk_trackball_settings_TrackballConfig){
        .schema_version = source->schema_version,
        .normal_cpi = source->normal_cpi,
        .precision_cpi = source->precision_cpi,
        .enabled = source->enabled,
        .selected_position = source->selected_position,
        .has_original_binding = true,
        .original_binding =
            {
                .behavior_id = source->original_binding.behavior_id,
                .param1 = source->original_binding.param1,
                .param2 = source->original_binding.param2,
            },
        .revision = source->revision,
        .precision_active = source->precision_active,
        .current_cpi = source->current_cpi,
    };
}

static struct zmk_rpc_custom_subsystem_meta trackball_settings_rpc_meta = {
    .ui_urls = NULL,
    .ui_urls_count = 0,
    .security = ZMK_STUDIO_RPC_HANDLER_UNSECURED,
};

ZMK_RPC_CUSTOM_SUBSYSTEM(trackball_settings, &trackball_settings_rpc_meta,
                         trackball_settings_rpc_handle_request);
ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER(trackball_settings, zmk_trackball_settings_Response);

static int trackball_settings_rpc_subsystem_index(void) {
    size_t subsystem_count;

    STRUCT_SECTION_COUNT(zmk_rpc_custom_subsystem, &subsystem_count);
    for (size_t index = 0; index < subsystem_count; index++) {
        struct zmk_rpc_custom_subsystem *subsystem;

        STRUCT_SECTION_GET(zmk_rpc_custom_subsystem, index, &subsystem);
        if (subsystem == &zmk_rpc_custom_subsystem_trackball_settings) {
            return (int)index;
        }
    }
    return -ENOENT;
}

static zmk_trackball_settings_Notification trackball_settings_rpc_notification;

static bool trackball_settings_rpc_encode_notification(pb_ostream_t *stream,
                                                        const pb_field_t *field,
                                                        void *const *arg) {
    const zmk_trackball_settings_Notification *notification = *arg;

    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }
    return pb_encode_submessage(stream, zmk_trackball_settings_Notification_fields, notification);
}

static void trackball_settings_rpc_production_notify(
    void *context, const struct trackball_settings_rpc_config *config) {
    const int subsystem_index = trackball_settings_rpc_subsystem_index();

    (void)context;
    if (subsystem_index < 0) {
        LOG_WRN("Trackball Settings custom subsystem is not registered");
        return;
    }

    trackball_settings_rpc_notification =
        (zmk_trackball_settings_Notification)zmk_trackball_settings_Notification_init_zero;
    trackball_settings_rpc_notification.which_notification_type =
        zmk_trackball_settings_Notification_changed_tag;
    trackball_settings_rpc_config_to_pb(
        &trackball_settings_rpc_notification.notification_type.changed, config);
    (void)raise_zmk_studio_custom_notification(
        (struct zmk_studio_custom_notification){
            .subsystem_index = (uint8_t)subsystem_index,
            .encode_payload =
                {
                    .funcs.encode = trackball_settings_rpc_encode_notification,
                    .arg = &trackball_settings_rpc_notification,
                },
        });
}

static const struct trackball_settings_rpc_service trackball_settings_rpc_production_service = {
    .context = NULL,
    .get_record = trackball_settings_rpc_production_get_record,
    .validate = trackball_settings_rpc_production_validate,
    .apply = trackball_settings_rpc_production_apply,
    .set_precision_active = trackball_settings_rpc_production_set_precision_active,
    .get_precision_snapshot = trackball_settings_rpc_production_get_precision_snapshot,
    .notify = trackball_settings_rpc_production_notify,
};

static int trackball_settings_rpc_request_from_pb(
    const zmk_trackball_settings_ApplyRequest *request,
    struct trackball_settings_request *destination) {
    if (request == NULL || destination == NULL) {
        return -EINVAL;
    }
    if (request->normal_cpi > UINT16_MAX || request->precision_cpi > UINT16_MAX) {
        return -ERANGE;
    }
    if (request->selected_position > UINT8_MAX) {
        return -EINVAL;
    }

    *destination = (struct trackball_settings_request){
        .normal_cpi = (uint16_t)request->normal_cpi,
        .precision_cpi = (uint16_t)request->precision_cpi,
        .enabled = request->enabled,
        .selected_position = (uint8_t)request->selected_position,
        .expected_revision = request->expected_revision,
    };
    return 0;
}

static void trackball_settings_rpc_apply_response_to_pb(
    zmk_trackball_settings_ApplyResponse *destination,
    const struct trackball_settings_rpc_response *source) {
    *destination =
        (zmk_trackball_settings_ApplyResponse)zmk_trackball_settings_ApplyResponse_init_zero;
    destination->result = (zmk_trackball_settings_ApplyResponse_Result)source->result;
    destination->has_config = true;
    trackball_settings_rpc_config_to_pb(&destination->config, &source->config);
}

static bool trackball_settings_rpc_handle_request(const zmk_custom_CallRequest *raw_request,
                                                  pb_callback_t *encode_response) {
    zmk_trackball_settings_Response *response =
        ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER_ALLOCATE(trackball_settings, encode_response);
    zmk_trackball_settings_Request request = zmk_trackball_settings_Request_init_zero;
    struct trackball_settings_rpc_response internal_response;
    struct trackball_settings_request internal_request;
    pb_istream_t request_stream;
    int err;

    request_stream = pb_istream_from_buffer(raw_request->payload.bytes, raw_request->payload.size);
    if (!pb_decode(&request_stream, zmk_trackball_settings_Request_fields, &request)) {
        LOG_WRN("Failed to decode trackball settings request: %s", PB_GET_ERROR(&request_stream));
        return false;
    }

    switch (request.which_request_type) {
    case zmk_trackball_settings_Request_get_tag:
        (void)trackball_settings_rpc_dispatch(&trackball_settings_rpc_production_service,
                                              TRACKBALL_SETTINGS_RPC_GET, NULL, &internal_response);
        response->which_response_type = zmk_trackball_settings_Response_get_tag;
        trackball_settings_rpc_config_to_pb(&response->response_type.get, &internal_response.config);
        return true;
    case zmk_trackball_settings_Request_validate_tag:
        err = trackball_settings_rpc_request_from_pb(&request.request_type.validate, &internal_request);
        if (err != 0) {
            trackball_settings_rpc_response_with_result(
                &trackball_settings_rpc_production_service, TRACKBALL_SETTINGS_RPC_VALIDATE,
                err == -ERANGE ? TRACKBALL_SETTINGS_RPC_INVALID_CPI
                               : TRACKBALL_SETTINGS_RPC_INVALID_POSITION,
                &internal_response);
        } else {
            (void)trackball_settings_rpc_dispatch(&trackball_settings_rpc_production_service,
                                                  TRACKBALL_SETTINGS_RPC_VALIDATE, &internal_request,
                                                  &internal_response);
        }
        response->which_response_type = zmk_trackball_settings_Response_validate_tag;
        trackball_settings_rpc_apply_response_to_pb(&response->response_type.validate,
                                                    &internal_response);
        return true;
    case zmk_trackball_settings_Request_apply_tag:
        err = trackball_settings_rpc_request_from_pb(&request.request_type.apply, &internal_request);
        if (err != 0) {
            trackball_settings_rpc_response_with_result(
                &trackball_settings_rpc_production_service, TRACKBALL_SETTINGS_RPC_APPLY,
                err == -ERANGE ? TRACKBALL_SETTINGS_RPC_INVALID_CPI
                               : TRACKBALL_SETTINGS_RPC_INVALID_POSITION,
                &internal_response);
        } else {
            (void)trackball_settings_rpc_dispatch(&trackball_settings_rpc_production_service,
                                                  TRACKBALL_SETTINGS_RPC_APPLY, &internal_request,
                                                  &internal_response);
        }
        response->which_response_type = zmk_trackball_settings_Response_apply_tag;
        trackball_settings_rpc_apply_response_to_pb(&response->response_type.apply,
                                                    &internal_response);
        return true;
    default:
        LOG_WRN("Unsupported trackball settings request: %d", request.which_request_type);
        return false;
    }
}

static int trackball_settings_rpc_layer_listener(const zmk_event_t *event) {
    const struct zmk_layer_state_changed *layer_changed = as_zmk_layer_state_changed(event);
    int err;

    if (layer_changed == NULL || layer_changed->layer != TRACKBALL_SETTINGS_PRECISION_LAYER) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    err = trackball_settings_rpc_precision_layer_changed(
        &trackball_settings_rpc_production_service, layer_changed->layer, layer_changed->state);
    if (err != 0) {
        LOG_WRN("Failed to apply precision layer state: %d", err);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(trackball_settings_rpc, trackball_settings_rpc_layer_listener);
ZMK_SUBSCRIPTION(trackball_settings_rpc, zmk_layer_state_changed);

#endif
