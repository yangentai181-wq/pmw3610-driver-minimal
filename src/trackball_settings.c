#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

#include <pmw3610/trackball_settings.h>

#if !defined(TRACKBALL_SETTINGS_HOST_TEST) && !defined(TRACKBALL_SETTINGS_TEST_ADAPTER)
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>

#include <zmk/behavior.h>
#include <zmk/keymap.h>
#endif

enum trackball_settings_binding_kind {
    TRACKBALL_SETTINGS_BINDING_UNSUPPORTED,
    TRACKBALL_SETTINGS_BINDING_KP,
    TRACKBALL_SETTINGS_BINDING_LT,
    TRACKBALL_SETTINGS_BINDING_MT,
    TRACKBALL_SETTINGS_BINDING_LT_MKP,
    TRACKBALL_SETTINGS_BINDING_TRANS,
};

struct trackball_settings_binding_info {
    struct zmk_behavior_binding binding;
    enum trackball_settings_binding_kind kind;
    uint16_t behavior_id;
};

struct trackball_settings_binding_snapshot {
    uint8_t position;
    struct zmk_behavior_binding binding;
};

int trackball_settings_read_record_exact(struct trackball_settings_record *record,
                                         trackball_settings_record_read_cb read_cb, void *cb_arg) {
    struct trackball_settings_record candidate;
    ssize_t bytes_read;

    if (record == NULL || read_cb == NULL) {
        return -EINVAL;
    }
    bytes_read = read_cb(cb_arg, &candidate, sizeof(candidate));
    if (bytes_read != (ssize_t)sizeof(candidate)) {
        return bytes_read < 0 ? (int)bytes_read : -EIO;
    }
    *record = candidate;
    return 0;
}

static const char *trackball_settings_behavior_name(enum trackball_settings_binding_kind kind) {
#if defined(TRACKBALL_SETTINGS_HOST_TEST) || defined(TRACKBALL_SETTINGS_TEST_ADAPTER)
    switch (kind) {
    case TRACKBALL_SETTINGS_BINDING_KP:
        return "kp";
    case TRACKBALL_SETTINGS_BINDING_LT:
        return "lt";
    case TRACKBALL_SETTINGS_BINDING_MT:
        return "mt";
    case TRACKBALL_SETTINGS_BINDING_LT_MKP:
        return "lt_mkp";
    case TRACKBALL_SETTINGS_BINDING_TRANS:
        return "trans";
    default:
        return NULL;
    }
#else
    switch (kind) {
    case TRACKBALL_SETTINGS_BINDING_KP:
        return DEVICE_DT_NAME(DT_NODELABEL(kp));
    case TRACKBALL_SETTINGS_BINDING_LT:
        return DEVICE_DT_NAME(DT_NODELABEL(lt));
    case TRACKBALL_SETTINGS_BINDING_MT:
        return DEVICE_DT_NAME(DT_NODELABEL(mt));
    case TRACKBALL_SETTINGS_BINDING_LT_MKP:
#if DT_NODE_EXISTS(DT_NODELABEL(lt_mkp))
        return DEVICE_DT_NAME(DT_NODELABEL(lt_mkp));
#else
        return NULL;
#endif
    case TRACKBALL_SETTINGS_BINDING_TRANS:
        return DEVICE_DT_NAME(DT_NODELABEL(trans));
    default:
        return NULL;
    }
#endif
}

static uint16_t trackball_settings_behavior_id(const char *name) {
#if defined(TRACKBALL_SETTINGS_HOST_TEST) || defined(TRACKBALL_SETTINGS_TEST_ADAPTER)
    if (name == NULL) {
        return UINT16_MAX;
    }
    if (strcmp(name, "kp") == 0) {
        return TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_KP;
    }
    if (strcmp(name, "lt") == 0) {
        return TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_LT;
    }
    if (strcmp(name, "mt") == 0) {
        return TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_MT;
    }
    if (strcmp(name, "lt_mkp") == 0) {
        return TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_LT_MKP;
    }
    return UINT16_MAX;
#else
    return zmk_behavior_get_local_id(name);
#endif
}

static const char *trackball_settings_behavior_name_from_id(uint16_t behavior_id) {
#if defined(TRACKBALL_SETTINGS_HOST_TEST) || defined(TRACKBALL_SETTINGS_TEST_ADAPTER)
    switch (behavior_id) {
    case TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_KP:
        return "kp";
    case TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_LT:
        return "lt";
    case TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_MT:
        return "mt";
    case TRACKBALL_SETTINGS_TEST_BEHAVIOR_ID_LT_MKP:
        return "lt_mkp";
    default:
        return NULL;
    }
#else
    return zmk_behavior_find_behavior_name_from_local_id(behavior_id);
#endif
}

static const char *trackball_settings_binding_name(const struct zmk_behavior_binding *binding) {
    if (binding == NULL) {
        return NULL;
    }
    if (binding->behavior_dev != NULL) {
        return binding->behavior_dev;
    }

#if !defined(TRACKBALL_SETTINGS_HOST_TEST) && !defined(TRACKBALL_SETTINGS_TEST_ADAPTER)
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_LOCAL_IDS_IN_BINDINGS)
    return zmk_behavior_find_behavior_name_from_local_id(binding->local_id);
#endif
#endif
    return NULL;
}

static enum trackball_settings_binding_kind
trackball_settings_binding_kind_from_name(const char *name) {
    if (name == NULL) {
        return TRACKBALL_SETTINGS_BINDING_UNSUPPORTED;
    }

    for (enum trackball_settings_binding_kind kind = TRACKBALL_SETTINGS_BINDING_KP;
         kind <= TRACKBALL_SETTINGS_BINDING_TRANS; kind++) {
        const char *candidate = trackball_settings_behavior_name(kind);
        if (candidate != NULL && strcmp(name, candidate) == 0) {
            return kind;
        }
    }
    return TRACKBALL_SETTINGS_BINDING_UNSUPPORTED;
}

static struct zmk_behavior_binding trackball_settings_make_binding(const char *name, uint16_t id,
                                                                    uint32_t param1,
                                                                    uint32_t param2) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = name,
        .param1 = param1,
        .param2 = param2,
    };

#if !defined(TRACKBALL_SETTINGS_HOST_TEST) && !defined(TRACKBALL_SETTINGS_TEST_ADAPTER)
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_LOCAL_IDS_IN_BINDINGS)
    binding.local_id = id;
#else
    (void)id;
#endif
#else
    (void)id;
#endif
    return binding;
}

static int trackball_settings_inspect_binding(const struct zmk_behavior_binding *binding,
                                              struct trackball_settings_binding_info *info) {
    const char *name;
    enum trackball_settings_binding_kind kind;
    uint16_t behavior_id;

    if (binding == NULL || info == NULL) {
        return -EINVAL;
    }
    name = trackball_settings_binding_name(binding);
    kind = trackball_settings_binding_kind_from_name(name);
    if (kind == TRACKBALL_SETTINGS_BINDING_UNSUPPORTED || kind == TRACKBALL_SETTINGS_BINDING_TRANS) {
        return -ENOTSUP;
    }
    if ((kind == TRACKBALL_SETTINGS_BINDING_LT ||
         kind == TRACKBALL_SETTINGS_BINDING_LT_MKP) &&
        binding->param1 == TRACKBALL_SETTINGS_PRECISION_LAYER) {
        return -ENOTSUP;
    }
    behavior_id = trackball_settings_behavior_id(name);
    if (behavior_id == UINT16_MAX) {
        return -ENOTSUP;
    }

    info->binding = *binding;
    info->binding.behavior_dev = name;
    info->kind = kind;
    info->behavior_id = behavior_id;
    return 0;
}

static int trackball_settings_binding_from_record(const struct trackball_settings_record *record,
                                                  struct trackball_settings_binding_info *info) {
    const char *name;
    enum trackball_settings_binding_kind kind;

    if (record == NULL || info == NULL || record->original_behavior_id == UINT16_MAX) {
        return -EINVAL;
    }
    name = trackball_settings_behavior_name_from_id(record->original_behavior_id);
    kind = trackball_settings_binding_kind_from_name(name);
    if (kind == TRACKBALL_SETTINGS_BINDING_UNSUPPORTED || kind == TRACKBALL_SETTINGS_BINDING_TRANS) {
        return -ENOTSUP;
    }
    if ((kind == TRACKBALL_SETTINGS_BINDING_LT ||
         kind == TRACKBALL_SETTINGS_BINDING_LT_MKP) &&
        record->original_param1 == TRACKBALL_SETTINGS_PRECISION_LAYER) {
        return -ENOTSUP;
    }

    info->binding = trackball_settings_make_binding(name, record->original_behavior_id,
                                                    record->original_param1,
                                                    record->original_param2);
    info->kind = kind;
    info->behavior_id = record->original_behavior_id;
    return 0;
}

static int trackball_settings_wrapper_from_original(const struct trackball_settings_binding_info *original,
                                                    struct zmk_behavior_binding *wrapper) {
    const char *wrapper_name;
    uint16_t wrapper_id;
    uint32_t tap_param;

    if (original == NULL || wrapper == NULL) {
        return -EINVAL;
    }
    switch (original->kind) {
    case TRACKBALL_SETTINGS_BINDING_KP:
        wrapper_name = trackball_settings_behavior_name(TRACKBALL_SETTINGS_BINDING_LT);
        tap_param = original->binding.param1;
        break;
    case TRACKBALL_SETTINGS_BINDING_LT:
    case TRACKBALL_SETTINGS_BINDING_MT:
        wrapper_name = trackball_settings_behavior_name(TRACKBALL_SETTINGS_BINDING_LT);
        tap_param = original->binding.param2;
        break;
    case TRACKBALL_SETTINGS_BINDING_LT_MKP:
        wrapper_name = trackball_settings_behavior_name(TRACKBALL_SETTINGS_BINDING_LT_MKP);
        tap_param = original->binding.param2;
        break;
    default:
        return -ENOTSUP;
    }
    wrapper_id = trackball_settings_behavior_id(wrapper_name);
    if (wrapper_name == NULL || wrapper_id == UINT16_MAX) {
        return -ENOTSUP;
    }

    *wrapper = trackball_settings_make_binding(wrapper_name, wrapper_id,
                                               TRACKBALL_SETTINGS_PRECISION_LAYER, tap_param);
    return 0;
}

static bool trackball_settings_bindings_match(const struct zmk_behavior_binding *actual,
                                              const struct zmk_behavior_binding *expected) {
    const char *actual_name;
    const char *expected_name;

    if (actual == NULL || expected == NULL) {
        return false;
    }
    actual_name = trackball_settings_binding_name(actual);
    expected_name = trackball_settings_binding_name(expected);
    return actual_name != NULL && expected_name != NULL && strcmp(actual_name, expected_name) == 0 &&
           actual->param1 == expected->param1 && actual->param2 == expected->param2;
}

#if !defined(TRACKBALL_SETTINGS_HOST_TEST) && !defined(TRACKBALL_SETTINGS_TEST_ADAPTER)
static bool trackball_settings_records_match(const struct trackball_settings_record *actual,
                                             const struct trackball_settings_record *expected) {
    return actual != NULL && expected != NULL &&
           actual->schema_version == expected->schema_version && actual->enabled == expected->enabled &&
           actual->selected_position == expected->selected_position &&
           actual->normal_cpi == expected->normal_cpi &&
           actual->precision_cpi == expected->precision_cpi &&
           actual->original_behavior_id == expected->original_behavior_id &&
           actual->original_param1 == expected->original_param1 &&
           actual->original_param2 == expected->original_param2 && actual->revision == expected->revision;
}
#endif

static bool trackball_settings_adapter_valid(const struct trackball_settings_adapter *adapter) {
    return adapter != NULL && adapter->get_binding != NULL && adapter->set_binding != NULL &&
           adapter->save_keymap != NULL && adapter->save_settings != NULL &&
           adapter->apply_profile != NULL;
}

static int trackball_settings_record_validate(const struct trackball_settings_record *record) {
    struct trackball_settings_binding_info original;

    if (record == NULL || record->schema_version != TRACKBALL_SETTINGS_SCHEMA_VERSION ||
        trackball_profile_validate(record->normal_cpi, record->precision_cpi) != 0) {
        return -EINVAL;
    }
    if (!record->enabled) {
        return 0;
    }
    return trackball_settings_binding_from_record(record, &original);
}

static int trackball_settings_current_wrapper(const struct trackball_settings_record *record,
                                              struct trackball_settings_binding_info *original,
                                              struct zmk_behavior_binding *wrapper) {
    int err = trackball_settings_binding_from_record(record, original);
    if (err != 0) {
        return err;
    }
    return trackball_settings_wrapper_from_original(original, wrapper);
}

int trackball_settings_validate(const struct trackball_settings_record *current,
                                const struct trackball_settings_request *request,
                                const struct trackball_settings_adapter *adapter) {
    const struct zmk_behavior_binding *binding;
    struct trackball_settings_binding_info original;
    struct zmk_behavior_binding wrapper;
    int err;

    if (current == NULL || request == NULL || !trackball_settings_adapter_valid(adapter)) {
        return -EINVAL;
    }
    err = trackball_settings_record_validate(current);
    if (err != 0) {
        return err;
    }
    if (request->expected_revision != current->revision) {
        return -ESTALE;
    }
    if (trackball_profile_validate(request->normal_cpi, request->precision_cpi) != 0) {
        return -EINVAL;
    }

    if (current->enabled) {
        err = trackball_settings_current_wrapper(current, &original, &wrapper);
        if (err != 0) {
            return err;
        }
        binding = adapter->get_binding(TRACKBALL_SETTINGS_BASE_LAYER, current->selected_position);
        if (!trackball_settings_bindings_match(binding, &wrapper)) {
            return -EINVAL;
        }
    }
    if (!request->enabled ||
        (current->enabled && request->selected_position == current->selected_position)) {
        return 0;
    }

    binding = adapter->get_binding(TRACKBALL_SETTINGS_BASE_LAYER, request->selected_position);
    return trackball_settings_inspect_binding(binding, &original);
}

static int trackball_settings_snapshot_binding(const struct trackball_settings_adapter *adapter,
                                               uint8_t position,
                                               struct trackball_settings_binding_snapshot *snapshot) {
    const struct zmk_behavior_binding *binding =
        adapter->get_binding(TRACKBALL_SETTINGS_BASE_LAYER, position);

    if (binding == NULL || snapshot == NULL) {
        return -EINVAL;
    }
    snapshot->position = position;
    snapshot->binding = *binding;
    return 0;
}

static void trackball_settings_rollback(
    const struct trackball_settings_adapter *adapter,
    const struct trackball_settings_binding_snapshot *snapshots, size_t snapshot_count,
    const struct trackball_settings_record *previous) {
    const struct trackball_profile previous_profile = {
        .normal_cpi = previous->normal_cpi,
        .precision_cpi = previous->precision_cpi,
    };

    for (size_t index = 0; index < snapshot_count; index++) {
        (void)adapter->set_binding(TRACKBALL_SETTINGS_BASE_LAYER, snapshots[index].position,
                                   snapshots[index].binding);
    }
    (void)adapter->apply_profile(&previous_profile);
    (void)adapter->save_keymap();
    (void)adapter->save_settings(previous);
}

static void trackball_settings_reload_rollback(
    const struct trackball_settings_adapter *adapter,
    const struct trackball_settings_binding_snapshot *snapshot,
    const struct trackball_profile *previous_profile) {
    if (snapshot != NULL) {
        (void)adapter->set_binding(TRACKBALL_SETTINGS_BASE_LAYER, snapshot->position,
                                   snapshot->binding);
    }
    (void)adapter->apply_profile(previous_profile);
}

static int trackball_settings_verify_readback(
    const struct trackball_settings_adapter *adapter, const struct trackball_settings_record *previous,
    const struct trackball_settings_request *request,
    const struct trackball_settings_binding_info *previous_original,
    const struct zmk_behavior_binding *new_wrapper) {
    const struct zmk_behavior_binding *binding;

    if (previous->enabled && (!request->enabled ||
                              request->selected_position != previous->selected_position)) {
        binding = adapter->get_binding(TRACKBALL_SETTINGS_BASE_LAYER, previous->selected_position);
        if (!trackball_settings_bindings_match(binding, &previous_original->binding)) {
            return -EIO;
        }
    }
    if (request->enabled) {
        binding = adapter->get_binding(TRACKBALL_SETTINGS_BASE_LAYER, request->selected_position);
        if (!trackball_settings_bindings_match(binding, new_wrapper)) {
            return -EIO;
        }
    }
    return 0;
}

int trackball_settings_apply(struct trackball_settings_record *current,
                             const struct trackball_settings_request *request,
                             const struct trackball_settings_adapter *adapter) {
    struct trackball_settings_record previous;
    struct trackball_settings_record next;
    struct trackball_settings_binding_info previous_original = {0};
    struct trackball_settings_binding_info selected_original = {0};
    struct zmk_behavior_binding new_wrapper = {0};
    struct trackball_settings_binding_snapshot snapshots[2];
    struct trackball_profile next_profile;
    size_t snapshot_count = 0;
    bool restore_previous;
    bool install_new;
    int err;

    err = trackball_settings_validate(current, request, adapter);
    if (err != 0) {
        return err;
    }
    if (current->revision == UINT32_MAX) {
        return -EOVERFLOW;
    }
    previous = *current;
    restore_previous = previous.enabled &&
                       (!request->enabled || request->selected_position != previous.selected_position);
    install_new = request->enabled &&
                  (!previous.enabled || request->selected_position != previous.selected_position);

    if (previous.enabled) {
        err = trackball_settings_current_wrapper(&previous, &previous_original, &new_wrapper);
        if (err != 0) {
            return err;
        }
        err = trackball_settings_snapshot_binding(adapter, previous.selected_position,
                                                  &snapshots[snapshot_count++]);
        if (err != 0) {
            return err;
        }
    }
    if (request->enabled) {
        if (previous.enabled && request->selected_position == previous.selected_position) {
            selected_original = previous_original;
        } else {
            const struct zmk_behavior_binding *binding =
                adapter->get_binding(TRACKBALL_SETTINGS_BASE_LAYER, request->selected_position);
            err = trackball_settings_inspect_binding(binding, &selected_original);
            if (err != 0) {
                return err;
            }
            err = trackball_settings_snapshot_binding(adapter, request->selected_position,
                                                      &snapshots[snapshot_count++]);
            if (err != 0) {
                return err;
            }
        }
        err = trackball_settings_wrapper_from_original(&selected_original, &new_wrapper);
        if (err != 0) {
            return err;
        }
    }

    next = (struct trackball_settings_record){
        .schema_version = TRACKBALL_SETTINGS_SCHEMA_VERSION,
        .enabled = request->enabled,
        .selected_position = request->enabled ? request->selected_position : 0,
        .normal_cpi = request->normal_cpi,
        .precision_cpi = request->precision_cpi,
        .revision = previous.revision + 1U,
    };
    if (request->enabled) {
        next.original_behavior_id = selected_original.behavior_id;
        next.original_param1 = selected_original.binding.param1;
        next.original_param2 = selected_original.binding.param2;
    }
    next_profile = (struct trackball_profile){
        .normal_cpi = next.normal_cpi,
        .precision_cpi = next.precision_cpi,
    };

    if (restore_previous) {
        err = adapter->set_binding(TRACKBALL_SETTINGS_BASE_LAYER, previous.selected_position,
                                   previous_original.binding);
        if (err != 0) {
            goto rollback;
        }
    }
    if (install_new) {
        err = adapter->set_binding(TRACKBALL_SETTINGS_BASE_LAYER, request->selected_position,
                                   new_wrapper);
        if (err != 0) {
            goto rollback;
        }
    }
    err = adapter->apply_profile(&next_profile);
    if (err != 0) {
        goto rollback;
    }
    err = adapter->save_keymap();
    if (err != 0) {
        goto rollback;
    }
    err = adapter->save_settings(&next);
    if (err != 0) {
        goto rollback;
    }
    err = trackball_settings_verify_readback(adapter, &previous, request, &previous_original,
                                             &new_wrapper);
    if (err != 0) {
        goto rollback;
    }

    *current = next;
    return 0;

rollback:
    trackball_settings_rollback(adapter, snapshots, snapshot_count, &previous);
    return err;
}

int trackball_settings_reload(const struct trackball_settings_record *record,
                              const struct trackball_settings_adapter *adapter) {
    struct trackball_settings_binding_info original;
    struct zmk_behavior_binding wrapper;
    struct trackball_settings_binding_snapshot snapshot;
    struct trackball_profile profile;
    struct trackball_profile previous_profile;
    int err;

    if (!trackball_settings_adapter_valid(adapter) || adapter->get_profile == NULL) {
        return -EINVAL;
    }
    err = trackball_settings_record_validate(record);
    if (err != 0) {
        return err;
    }
    profile = (struct trackball_profile){
        .normal_cpi = record->normal_cpi,
        .precision_cpi = record->precision_cpi,
    };
    err = adapter->get_profile(&previous_profile);
    if (err != 0) {
        return err;
    }
    if (!record->enabled) {
        err = adapter->apply_profile(&profile);
        if (err != 0) {
            trackball_settings_reload_rollback(adapter, NULL, &previous_profile);
        }
        return err;
    }
    err = trackball_settings_binding_from_record(record, &original);
    if (err != 0) {
        return err;
    }
    err = trackball_settings_wrapper_from_original(&original, &wrapper);
    if (err != 0) {
        return err;
    }
    err = trackball_settings_snapshot_binding(adapter, record->selected_position, &snapshot);
    if (err != 0) {
        return err;
    }
    err = adapter->set_binding(TRACKBALL_SETTINGS_BASE_LAYER, record->selected_position, wrapper);
    if (err != 0) {
        goto rollback;
    }
    err = adapter->apply_profile(&profile);
    if (err != 0) {
        goto rollback;
    }
    if (!trackball_settings_bindings_match(
            adapter->get_binding(TRACKBALL_SETTINGS_BASE_LAYER, record->selected_position), &wrapper)) {
        err = -EIO;
        goto rollback;
    }

    return 0;

rollback:
    trackball_settings_reload_rollback(adapter, &snapshot, &previous_profile);
    return err;
}

#if !defined(TRACKBALL_SETTINGS_HOST_TEST) && !defined(TRACKBALL_SETTINGS_TEST_ADAPTER)

#define TRACKBALL_SETTINGS_STORAGE_KEY "trackball/settings"

static struct trackball_settings_record trackball_settings_current = {
    .schema_version = TRACKBALL_SETTINGS_SCHEMA_VERSION,
    .enabled = false,
    .selected_position = 0,
    .normal_cpi = CONFIG_PMW3610_CPI,
    .precision_cpi = CONFIG_PMW3610_SNIPE_CPI,
    .revision = 0,
};

K_MUTEX_DEFINE(trackball_settings_lock);

static const struct zmk_behavior_binding *trackball_settings_get_keymap_binding(uint8_t layer,
                                                                                  uint8_t position) {
    return zmk_keymap_get_layer_binding_at_idx(layer, position);
}

static int trackball_settings_set_keymap_binding(uint8_t layer, uint8_t position,
                                                 struct zmk_behavior_binding binding) {
    return zmk_keymap_set_layer_binding_at_idx(layer, position, binding);
}

static int trackball_settings_save_keymap(void) {
    return zmk_keymap_save_changes();
}

static int trackball_settings_get_current_profile(struct trackball_profile *profile) {
    if (profile == NULL) {
        return -EINVAL;
    }

    *profile = (struct trackball_profile){
        .normal_cpi = trackball_settings_current.normal_cpi,
        .precision_cpi = trackball_settings_current.precision_cpi,
    };
    return 0;
}

struct trackball_settings_storage_readback {
    struct trackball_settings_record record;
    int error;
    bool found;
};

static int trackball_settings_readback_set(const char *name, size_t len, settings_read_cb read_cb,
                                           void *cb_arg, void *param) {
    const char *next;
    struct trackball_settings_storage_readback *readback = param;
    int err;

    if (!settings_name_steq(name, "settings", &next) || next != NULL) {
        return 0;
    }
    readback->found = true;
    if (len != sizeof(readback->record)) {
        readback->error = -EINVAL;
        return 1;
    }
    err = trackball_settings_read_record_exact(&readback->record, read_cb, cb_arg);
    if (err != 0) {
        readback->error = err;
    }
    return 1;
}

static int trackball_settings_read_saved_record(struct trackball_settings_record *record) {
    struct trackball_settings_storage_readback readback = {0};
    int err;

    err = settings_load_subtree_direct("trackball", trackball_settings_readback_set, &readback);
    if (err != 0) {
        return err;
    }
    if (!readback.found) {
        return -ENOENT;
    }
    if (readback.error != 0) {
        return readback.error;
    }
    *record = readback.record;
    return 0;
}

static int trackball_settings_save_record(const struct trackball_settings_record *record) {
    struct trackball_settings_record readback;
    int err;

    err = settings_save_one(TRACKBALL_SETTINGS_STORAGE_KEY, record, sizeof(*record));
    if (err != 0) {
        return err;
    }
    err = trackball_settings_read_saved_record(&readback);
    if (err != 0) {
        return err;
    }
    return trackball_settings_records_match(&readback, record) ? 0 : -EIO;
}

static const struct trackball_settings_adapter trackball_settings_zephyr_adapter = {
    .get_binding = trackball_settings_get_keymap_binding,
    .set_binding = trackball_settings_set_keymap_binding,
    .save_keymap = trackball_settings_save_keymap,
    .save_settings = trackball_settings_save_record,
    .apply_profile = pmw3610_apply_profile,
    .get_profile = trackball_settings_get_current_profile,
};

int trackball_settings_get_record(struct trackball_settings_record *record) {
    int err;

    if (record == NULL) {
        return -EINVAL;
    }
    err = k_mutex_lock(&trackball_settings_lock, K_FOREVER);
    if (err != 0) {
        return err;
    }
    *record = trackball_settings_current;
    (void)k_mutex_unlock(&trackball_settings_lock);
    return 0;
}

int trackball_settings_validate_request(const struct trackball_settings_request *request) {
    int err;

    if (request == NULL) {
        return -EINVAL;
    }
    err = k_mutex_lock(&trackball_settings_lock, K_FOREVER);
    if (err != 0) {
        return err;
    }
    err = trackball_settings_validate(&trackball_settings_current, request,
                                      &trackball_settings_zephyr_adapter);
    (void)k_mutex_unlock(&trackball_settings_lock);
    return err;
}

int trackball_settings_apply_request(const struct trackball_settings_request *request) {
    int err;

    if (request == NULL) {
        return -EINVAL;
    }
    err = k_mutex_lock(&trackball_settings_lock, K_FOREVER);
    if (err != 0) {
        return err;
    }
    err = trackball_settings_apply(&trackball_settings_current, request,
                                   &trackball_settings_zephyr_adapter);
    (void)k_mutex_unlock(&trackball_settings_lock);
    return err;
}

static int trackball_settings_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                           void *cb_arg) {
    const char *next;
    struct trackball_settings_record record;
    int err;

    if (!settings_name_steq(name, "settings", &next) || next != NULL) {
        return -ENOENT;
    }
    if (len != sizeof(record)) {
        return -EINVAL;
    }
    err = trackball_settings_read_record_exact(&record, read_cb, cb_arg);
    if (err != 0) {
        return err;
    }
    err = k_mutex_lock(&trackball_settings_lock, K_FOREVER);
    if (err != 0) {
        return err;
    }
    err = trackball_settings_reload(&record, &trackball_settings_zephyr_adapter);
    if (err == 0) {
        trackball_settings_current = record;
    }
    (void)k_mutex_unlock(&trackball_settings_lock);
    return err;
}

static struct settings_handler trackball_settings_handler = {
    .name = "trackball",
    .h_set = trackball_settings_settings_set,
};

static int trackball_settings_init(void) {
    int err;

    err = settings_subsys_init();
    if (err != 0) {
        return err;
    }
    err = settings_register(&trackball_settings_handler);
    if (err != 0) {
        return err;
    }
    return settings_load_subtree("trackball");
}

SYS_INIT(trackball_settings_init, APPLICATION, 92);

#else

int trackball_settings_get_record(struct trackball_settings_record *record) {
    (void)record;
    return -ENOTSUP;
}

int trackball_settings_validate_request(const struct trackball_settings_request *request) {
    (void)request;
    return -ENOTSUP;
}

int trackball_settings_apply_request(const struct trackball_settings_request *request) {
    (void)request;
    return -ENOTSUP;
}

#endif
