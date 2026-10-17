/* SPDX-License-Identifier: Apache-2.0 */
#include "bk7259_factory_reset.h"

#include <easyflash.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define INTENT "mybot.factory_reset.v1"
#define AUTH "mybot.device_auth"
#define WIFI "mybot.wifi.v1"
#define VOLUME "mybot.volume.v1"
#define UNRELATED "vendor.calibration"
#define RECORD_COUNT 5
#define BLOB_CAPACITY 64

typedef struct {
    const char *key;
    bool present;
    struct env_node_obj metadata;
    unsigned char blob[BLOB_CAPACITY];
} fake_record_t;

typedef enum {
    DELETE_NORMAL,
    DELETE_ERROR_BEFORE,
    DELETE_FALSE_SUCCESS,
    DELETE_ERROR_AFTER,
} delete_fault_t;

static fake_record_t records[RECORD_COUNT];
static EfErrCode init_result;
static unsigned init_calls;
static unsigned set_calls;
static unsigned delete_calls;
static const char *deleted_keys[16];
static EfErrCode set_result;
static bool drop_write;
static bool corrupt_write;
static bool drop_read;
static bool corrupt_read;
static int reported_read_delta;
static int reported_saved_delta;
static const char *delete_fault_key;
static delete_fault_t delete_fault;
static int portal_result;
static int sta_result;
static unsigned network_calls;
static char network_order[3];

static fake_record_t *lookup(const char *key) {
    for (size_t i = 0; i < RECORD_COUNT; ++i) {
        if (strcmp(records[i].key, key) == 0) {
            return &records[i];
        }
    }
    assert(!"unexpected storage key");
    return NULL;
}

static void store(const char *key, const void *blob, size_t length) {
    fake_record_t *record = lookup(key);
    assert(length <= sizeof(record->blob));
    record->present = true;
    memset(&record->metadata, 0, sizeof(record->metadata));
    record->metadata.status = ENV_WRITE;
    record->metadata.crc_is_ok = true;
    record->metadata.name_len = (uint8_t)strlen(key);
    record->metadata.value_len = (uint32_t)length;
    memcpy(record->metadata.name, key, strlen(key));
    memset(record->blob, 0, sizeof(record->blob));
    memcpy(record->blob, blob, length);
}

static void reset_fixture(void) {
    memset(records, 0, sizeof(records));
    records[0].key = INTENT;
    records[1].key = AUTH;
    records[2].key = WIFI;
    records[3].key = VOLUME;
    records[4].key = UNRELATED;
    init_result = EF_NO_ERR;
    init_calls = 0;
    set_calls = 0;
    delete_calls = 0;
    memset(deleted_keys, 0, sizeof(deleted_keys));
    set_result = EF_NO_ERR;
    drop_write = false;
    corrupt_write = false;
    drop_read = false;
    corrupt_read = false;
    reported_read_delta = 0;
    reported_saved_delta = 0;
    delete_fault_key = NULL;
    delete_fault = DELETE_NORMAL;
    portal_result = 0;
    sta_result = 0;
    network_calls = 0;
    memset(network_order, 0, sizeof(network_order));
    static const unsigned char value[] = {0x31, 0x42, 0x53, 0x64};
    store(AUTH, value, sizeof(value));
    store(WIFI, value, sizeof(value));
    store(VOLUME, value, sizeof(value));
    store(UNRELATED, value, sizeof(value));
}

EfErrCode easyflash_init(void) {
    ++init_calls;
    return init_result;
}

bool ef_get_env_obj(const char *key, env_node_obj_t node) {
    fake_record_t *record = lookup(key);
    if (!record->present) {
        return false;
    }
    *node = record->metadata;
    return true;
}

size_t ef_get_env_blob(const char *key, void *buffer, size_t capacity, size_t *saved) {
    fake_record_t *record = lookup(key);
    assert(record->present);
    size_t length = record->metadata.value_len;
    assert(length <= sizeof(record->blob));
    assert(capacity >= length);
    *saved = (size_t)((int)length + reported_saved_delta);
    if (!drop_read) {
        memcpy(buffer, record->blob, length);
        if (corrupt_read && length != 0) {
            ((unsigned char *)buffer)[0] ^= 1;
        }
    }
    return (size_t)((int)length + reported_read_delta);
}

EfErrCode ef_set_env_blob(const char *key, const void *buffer, size_t length) {
    assert(strcmp(key, INTENT) == 0);
    ++set_calls;
    if (set_result != EF_NO_ERR) {
        return set_result;
    }
    if (!drop_write) {
        store(key, buffer, length);
        if (corrupt_write) {
            lookup(key)->blob[0] ^= 1;
        }
    }
    return EF_NO_ERR;
}

EfErrCode ef_del_env(const char *key) {
    assert(strcmp(key, UNRELATED) != 0);
    assert(delete_calls < sizeof(deleted_keys) / sizeof(deleted_keys[0]));
    deleted_keys[delete_calls++] = key;
    delete_fault_t fault = DELETE_NORMAL;
    if (delete_fault_key != NULL && strcmp(key, delete_fault_key) == 0) {
        fault = delete_fault;
    }
    if (fault == DELETE_ERROR_BEFORE) {
        return EF_ERASE_ERR;
    }
    if (fault != DELETE_FALSE_SUCCESS) {
        lookup(key)->present = false;
    }
    return fault == DELETE_ERROR_AFTER ? EF_ERASE_ERR : EF_NO_ERR;
}

void aosl_log(int level, const char *format, ...) {
    (void)level;
    (void)format;
}

int bk7259_wifi_portal_stop(void) {
    assert(network_calls < 2);
    network_order[network_calls++] = 'P';
    return portal_result;
}

int bk7259_wifi_manager_disconnect(void) {
    assert(network_calls < 2);
    network_order[network_calls++] = 'S';
    return sta_result;
}

static void assert_product_records(bool present) {
    assert(lookup(AUTH)->present == present);
    assert(lookup(WIFI)->present == present);
    assert(lookup(VOLUME)->present == present);
    assert(lookup(UNRELATED)->present);
    static const unsigned char original[] = {0x31, 0x42, 0x53, 0x64};
    assert(memcmp(lookup(UNRELATED)->blob, original, sizeof(original)) == 0);
}

static void test_intent_and_boot_resume(void) {
    reset_fixture();
    assert(bk7259_factory_reset_pending() == 0);
    assert(bk7259_factory_reset_clear() == -1);
    assert(delete_calls == 0);
    assert_product_records(true);
    assert(bk7259_factory_reset_begin() == 0);
    assert(set_calls == 1);
    assert(bk7259_factory_reset_pending() == 1);
    /* No RAM state is needed to resume; repeat the boot-facing probe. */
    assert(bk7259_factory_reset_pending() == 1);
    assert(bk7259_factory_reset_begin() == 0);
    assert(set_calls == 1);
    assert(bk7259_factory_reset_clear() == 0);
    assert(delete_calls == 4);
    assert(strcmp(deleted_keys[0], AUTH) == 0);
    assert(strcmp(deleted_keys[1], WIFI) == 0);
    assert(strcmp(deleted_keys[2], VOLUME) == 0);
    assert(strcmp(deleted_keys[3], INTENT) == 0);
    assert_product_records(false);
    assert(bk7259_factory_reset_pending() == 0);
    assert(bk7259_factory_reset_clear() == -1);
    assert(delete_calls == 4);
    /* A fresh explicit reset is idempotent even if every product key is absent. */
    assert(bk7259_factory_reset_begin() == 0);
    assert(bk7259_factory_reset_clear() == 0);
    assert(delete_calls == 5);
}

static void test_storage_initialization_failure(void) {
    reset_fixture();
    init_result = EF_ENV_INIT_FAILED;
    assert(bk7259_factory_reset_pending() == -1);
    assert(bk7259_factory_reset_begin() == -1);
    assert(bk7259_factory_reset_clear() == -1);
    assert(init_calls == 3);
    assert(set_calls == 0 && delete_calls == 0);
    assert_product_records(true);
}

static void test_bad_marker(void) {
    for (unsigned failure = 0; failure < 12; ++failure) {
        reset_fixture();
        assert(bk7259_factory_reset_begin() == 0);
        fake_record_t *intent = lookup(INTENT);
        switch (failure) {
        case 0: intent->metadata.crc_is_ok = false; break;
        case 1: intent->metadata.status = ENV_PRE_WRITE; break;
        case 2: --intent->metadata.name_len; break;
        case 3: intent->metadata.name[0] ^= 1; break;
        case 4: --intent->metadata.value_len; break;
        case 5: ++intent->metadata.value_len; break;
        case 6: intent->blob[0] ^= 1; break;
        case 7: intent->blob[4] ^= 1; break; /* version */
        case 8: intent->blob[8] ^= 1; break; /* inverse magic */
        case 9: drop_read = true; break; /* low-level read lies about success */
        case 10: reported_read_delta = -1; break;
        case 11: reported_saved_delta = -1; break;
        default: assert(false);
        }
        assert(bk7259_factory_reset_pending() == -1);
        assert(bk7259_factory_reset_clear() == -1);
        assert(delete_calls == 0);
        assert_product_records(true);
        drop_read = false;
        reported_read_delta = 0;
        reported_saved_delta = 0;
        /* Boot rejects corrupt intent. A new explicit request may repair it. */
        assert(bk7259_factory_reset_begin() == 0);
        assert(bk7259_factory_reset_pending() == 1);
        assert(bk7259_factory_reset_clear() == 0);
        assert_product_records(false);
    }
}

static void test_write_readback_failures(void) {
    for (unsigned failure = 0; failure < 6; ++failure) {
        reset_fixture();
        switch (failure) {
        case 0: set_result = EF_WRITE_ERR; break;
        case 1: drop_write = true; break;
        case 2: corrupt_write = true; break;
        case 3: corrupt_read = true; break;
        case 4: reported_read_delta = -1; break;
        case 5: reported_saved_delta = -1; break;
        default: assert(false);
        }
        assert(bk7259_factory_reset_begin() == -1);
        assert(delete_calls == 0);
        assert_product_records(true);
        set_result = EF_NO_ERR;
        drop_write = false;
        corrupt_write = false;
        corrupt_read = false;
        reported_read_delta = 0;
        reported_saved_delta = 0;
        assert(bk7259_factory_reset_begin() == 0);
        assert(bk7259_factory_reset_clear() == 0);
    }
}

static void test_product_delete_failures(void) {
    static const char *const product_keys[] = {AUTH, WIFI, VOLUME};
    for (unsigned key = 0; key < 3; ++key) {
        for (delete_fault_t fault = DELETE_ERROR_BEFORE;
             fault <= DELETE_ERROR_AFTER; ++fault) {
            reset_fixture();
            assert(bk7259_factory_reset_begin() == 0);
            delete_fault_key = product_keys[key];
            delete_fault = fault;
            assert(bk7259_factory_reset_clear() == -1);
            assert(bk7259_factory_reset_pending() == 1);
            assert(lookup(UNRELATED)->present);
            assert(delete_calls == key + 1);
            delete_fault = DELETE_NORMAL;
            assert(bk7259_factory_reset_begin() == 0);
            assert(bk7259_factory_reset_clear() == 0);
            assert_product_records(false);
            assert(bk7259_factory_reset_pending() == 0);
        }
    }
}

static void test_final_marker_delete_failures(void) {
    for (delete_fault_t fault = DELETE_ERROR_BEFORE;
         fault <= DELETE_ERROR_AFTER; ++fault) {
        reset_fixture();
        assert(bk7259_factory_reset_begin() == 0);
        delete_fault_key = INTENT;
        delete_fault = fault;
        assert(bk7259_factory_reset_clear() == -1);
        assert_product_records(false);
        assert(set_calls == 2); /* also when deletion really removed the marker */
        assert(bk7259_factory_reset_pending() == 1);
        delete_fault = DELETE_NORMAL;
        assert(bk7259_factory_reset_clear() == 0);
        assert(bk7259_factory_reset_pending() == 0);
    }
    reset_fixture();
    assert(bk7259_factory_reset_begin() == 0);
    delete_fault_key = INTENT;
    delete_fault = DELETE_ERROR_AFTER;
    set_result = EF_WRITE_ERR;
    /* A failed marker repair must remain a failure; no automatic success. */
    assert(bk7259_factory_reset_clear() == -1);
    assert_product_records(false);
    assert(bk7259_factory_reset_pending() == 0);
    set_result = EF_NO_ERR;
    delete_fault = DELETE_NORMAL;
    assert(bk7259_factory_reset_begin() == 0);
    assert(bk7259_factory_reset_clear() == 0);
}

static void test_corrupt_owned_record_cleanup(void) {
    reset_fixture();
    lookup(AUTH)->metadata.crc_is_ok = false;
    lookup(WIFI)->metadata.status = ENV_PRE_DELETE;
    lookup(VOLUME)->metadata.name[0] ^= 1;
    assert(bk7259_factory_reset_begin() == 0);
    assert(bk7259_factory_reset_clear() == 0);
    assert_product_records(false);
}

static void test_network_disconnect_failures(void) {
    static const int portal_results[] = {0, -7, 0, -7};
    static const int sta_results[] = {0, 0, -9, -9};
    static const int expected[] = {0, -7, -1, -1};
    for (unsigned test = 0; test < 4; ++test) {
        reset_fixture();
        portal_result = portal_results[test];
        sta_result = sta_results[test];
        assert(bk7259_factory_reset_disconnect_network() == expected[test]);
        assert(network_calls == 2);
        assert(strcmp(network_order, "PS") == 0);
        assert_product_records(true);
    }
}

int main(void) {
    test_intent_and_boot_resume();
    test_storage_initialization_failure();
    test_bad_marker();
    test_write_readback_failures();
    test_product_delete_failures();
    test_final_marker_delete_failures();
    test_corrupt_owned_record_cleanup();
    test_network_disconnect_failures();
    puts("Factory-reset storage: intent, deletion, boot resume and network faults passed");
    return 0;
}
