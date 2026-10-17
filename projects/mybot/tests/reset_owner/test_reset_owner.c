/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Exercise the actual application owner, including its static reset loop. */
#define main mybot_firmware_main
#include "../../ap/ap_main.c"
#undef main

enum { EXIT_REBOOT = 1, EXIT_IDLE, EXIT_PROVISION };
static jmp_buf exit_point;
static char events[256];
static size_t event_count;
static mybot_state_t sdk_state;
static bool sdk_running;
static bool reset_request;
static int pending_result;
static unsigned pending_calls;
static unsigned begin_calls;
static unsigned stop_calls;
static unsigned disconnect_calls;
static unsigned clear_calls;
static unsigned reboot_calls;
static unsigned start_calls;
static unsigned provision_calls;
static unsigned shutdown_calls;
static unsigned failed_screens;
static unsigned restoring_screens;
static unsigned rebooting_screens;
static unsigned idle_polls;
static unsigned inject_retry_after;
static unsigned stop_failures;
static bool stop_failure_leaves_running;
static unsigned begin_failures;
static unsigned disconnect_failures;
static unsigned clear_failures;
static bool request_provision_when_running;
static bool request_reset_during_network;
static bool network_failed;
static bool start_failed;
static bool disconnect_succeeded;
static unsigned recovery_waits;
static unsigned inject_provision_after;
static bool inject_s2_on_reset_failure;
static bool queued_provision;
static unsigned discarded_provision;
volatile const char build_version[] = "host-test";

static void record(char event) {
    assert(event_count + 1 < sizeof(events));
    events[event_count++] = event;
    events[event_count] = '\0';
}

static void reset_fixture(void) {
    memset(events, 0, sizeof(events));
    event_count = 0;
    sdk_state = MYBOT_STATE_STOPPED;
    sdk_running = false;
    reset_request = false;
    pending_result = 0;
    pending_calls = begin_calls = stop_calls = disconnect_calls = clear_calls = 0;
    reboot_calls = start_calls = provision_calls = shutdown_calls = 0;
    failed_screens = restoring_screens = rebooting_screens = idle_polls = 0;
    inject_retry_after = stop_failures = begin_failures = 0;
    stop_failure_leaves_running = false;
    disconnect_failures = clear_failures = 0;
    request_provision_when_running = false;
    request_reset_during_network = false;
    network_failed = start_failed = disconnect_succeeded = false;
    recovery_waits = inject_provision_after = 0;
    inject_s2_on_reset_failure = queued_provision = false;
    discarded_provision = 0;
}

void test_log(const char *format, ...) {
    (void)format;
}

int bk7259_lcd_set_runtime_status(const char *state, bool in_conversation) {
    (void)state;
    (void)in_conversation;
    return 0;
}

int bk7259_lcd_show_factory_reset(bk7259_factory_reset_screen_t screen) {
    switch (screen) {
    case BK7259_FACTORY_RESET_RESTORING: ++restoring_screens; break;
    case BK7259_FACTORY_RESET_FAILED:
        ++failed_screens;
        if (inject_s2_on_reset_failure) {
            queued_provision = true;
        }
        break;
    case BK7259_FACTORY_RESET_REBOOTING: ++rebooting_screens; break;
    default: assert(false);
    }
    return 0;
}

bool bk7259_factory_reset_requested(void) {
    return reset_request;
}

bool bk7259_factory_reset_take_request(void) {
    bool result = reset_request;
    reset_request = false;
    return result;
}

int bk7259_factory_reset_pending(void) {
    record('Q');
    ++pending_calls;
    return pending_result;
}

int bk7259_factory_reset_begin(void) {
    record('B');
    ++begin_calls;
    if (begin_failures != 0) {
        --begin_failures;
        return -1;
    }
    pending_result = 1;
    return 0;
}

int bk7259_factory_reset_disconnect_network(void) {
    record('D');
    ++disconnect_calls;
    assert(!sdk_running && sdk_state == MYBOT_STATE_STOPPED);
    if (disconnect_failures != 0) {
        --disconnect_failures;
        return -1;
    }
    disconnect_succeeded = true;
    return 0;
}

int bk7259_factory_reset_clear(void) {
    record('C');
    ++clear_calls;
    assert(!sdk_running && sdk_state == MYBOT_STATE_STOPPED);
    assert(disconnect_succeeded);
    assert(pending_result == 1);
    if (clear_failures != 0) {
        --clear_failures;
        return -1;
    }
    pending_result = 0;
    return 0;
}

int mybot_start(const mybot_config_t *config) {
    assert(config != NULL && config->device_id[0] != '\0');
    assert(!sdk_running && sdk_state == MYBOT_STATE_STOPPED);
    record('A');
    ++start_calls;
    if (start_failed) {
        return -1;
    }
    sdk_running = true;
    sdk_state = MYBOT_STATE_READY;
    return 0;
}

void mybot_stop(void) {
    record('S');
    ++stop_calls;
    if (stop_failures != 0) {
        --stop_failures;
        sdk_running = stop_failure_leaves_running;
        sdk_state = stop_failure_leaves_running ? MYBOT_STATE_STOPPED : MYBOT_STATE_STOPPING;
    } else {
        sdk_running = false;
        sdk_state = MYBOT_STATE_STOPPED;
    }
}

bool mybot_is_running(void) {
    return sdk_running;
}

mybot_state_t mybot_get_state(void) {
    return sdk_state;
}

bool mybot_bk7259_wait_provision_request(uint32_t timeout_ms) {
    if (queued_provision) {
        queued_provision = false;
        ++discarded_provision;
        return true;
    }
    if (request_provision_when_running && sdk_running) {
        request_provision_when_running = false;
        return true;
    }
    if (timeout_ms != 0) {
        ++recovery_waits;
        if (inject_provision_after != 0 && recovery_waits == inject_provision_after) {
            return true;
        }
        if (recovery_waits >= 4) {
            longjmp(exit_point, EXIT_IDLE);
        }
    }
    return false;
}

int mybot_bk7259_ensure_network(const char *device_id) {
    assert(device_id != NULL && device_id[0] != '\0');
    record('N');
    if (request_reset_during_network) {
        reset_request = true;
        return BK7259_NETWORK_CANCELLED;
    }
    return network_failed ? -1 : 0;
}

int mybot_bk7259_provision_wifi(const char *device_id) {
    assert(device_id != NULL && device_id[0] != '\0');
    assert(!sdk_running && sdk_state == MYBOT_STATE_STOPPED);
    record('V');
    ++provision_calls;
    longjmp(exit_point, EXIT_PROVISION);
}

int mybot_bk7259_platform_prepare(void) {
    record('P');
    return 0;
}

int mybot_bk7259_platform_register(void) {
    record('G');
    return 0;
}

void mybot_bk7259_platform_shutdown(void) {
    ++shutdown_calls;
}

int rtos_delay_milliseconds(uint32_t milliseconds) {
    if (milliseconds == MYBOT_POLL_INTERVAL_MS) {
        ++idle_polls;
        if (inject_retry_after != 0 && idle_polls == inject_retry_after) {
            reset_request = true;
        } else if (idle_polls >= 4) {
            longjmp(exit_point, EXIT_IDLE);
        }
    }
    return 0;
}

void bk_reboot(void) {
    record('R');
    ++reboot_calls;
    longjmp(exit_point, EXIT_REBOOT);
}

int bk_init(void) { return 0; }
int media_service_init(void) { return 0; }
void aosl_ctor(void) {}
void aosl_dtor(void) {}
void aosl_set_log_level(int level) { (void)level; }
int rtos_delete_thread(beken_thread_t *thread) { (void)thread; return 0; }

int rtos_create_psram_thread(beken_thread_t *thread, uint8_t priority, const char *name,
                             beken_thread_function_t function, uint32_t stack_size,
                             beken_thread_arg_t arg) {
    (void)thread;
    (void)priority;
    (void)name;
    (void)function;
    (void)stack_size;
    (void)arg;
    return 0;
}

int bk_uid_get_data(uint8_t *uid) {
    memset(uid, 0x21, 32);
    return 0;
}

const mbedtls_md_info_t *mbedtls_md_info_from_type(int type) {
    static const mbedtls_md_info_t info;
    assert(type == MBEDTLS_MD_SHA256);
    return &info;
}

int mbedtls_md_hmac(const mbedtls_md_info_t *info, const unsigned char *key,
                    size_t key_length, const unsigned char *input,
                    size_t input_length, unsigned char *output) {
    (void)info;
    (void)key;
    (void)key_length;
    (void)input;
    (void)input_length;
    memset(output, 0x42, 32);
    return 0;
}

static void run_reset_loop(void) {
    int reason = setjmp(exit_point);
    if (reason == 0) {
        mybot_factory_reset_loop(true);
        assert(!"reset owner loop returned");
    }
}

static void test_success_order(void) {
    reset_fixture();
    sdk_state = MYBOT_STATE_IN_CONVERSATION;
    sdk_running = true;
    run_reset_loop();
    assert(strcmp(events, "BSDCR") == 0);
    assert(restoring_screens == 1 && rebooting_screens == 1 && failed_screens == 0);
    assert(reboot_calls == 1 && pending_result == 0);
}

static void test_stopped_guard_and_explicit_retry(void) {
    for (unsigned running = 0; running < 2; ++running) {
        reset_fixture();
        sdk_state = MYBOT_STATE_IN_CONVERSATION;
        sdk_running = true;
        stop_failures = 1;
        stop_failure_leaves_running = running;
        run_reset_loop();
        assert(strcmp(events, "BS") == 0);
        assert(begin_calls == 1 && stop_calls == 1);
        assert(disconnect_calls == 0 && clear_calls == 0 && reboot_calls == 0);
        assert(failed_screens == 1 && idle_polls == 4);
        reset_request = true;
        inject_retry_after = 0;
        idle_polls = 0;
        run_reset_loop();
        assert(strcmp(events, "BSBSDCR") == 0);
        assert(reboot_calls == 1);
    }
}

static void test_failed_step_requires_fresh_request(void) {
    for (unsigned failure = 0; failure < 3; ++failure) {
        reset_fixture();
        begin_failures = failure == 0;
        disconnect_failures = failure == 1;
        clear_failures = failure == 2;
        run_reset_loop();
        assert(begin_calls == 1 && stop_calls == 1 && reboot_calls == 0);
        assert(failed_screens == 1 && idle_polls == 4);
        if (failure < 2) {
            assert(clear_calls == 0);
        }
        reset_request = true;
        idle_polls = 0;
        run_reset_loop();
        assert(begin_calls == 2 && reboot_calls == 1);
    }
}

static void test_delayed_new_key_retries_once(void) {
    reset_fixture();
    disconnect_failures = 1;
    inject_retry_after = 2;
    run_reset_loop();
    assert(idle_polls == 2);
    assert(begin_calls == 2 && stop_calls == 2 && disconnect_calls == 2);
    assert(clear_calls == 1 && reboot_calls == 1);
}

static void test_boot_resume_before_network(void) {
    reset_fixture();
    pending_result = 1;
    int reason = setjmp(exit_point);
    if (reason == 0) {
        (void)mybot_run();
        assert(!"boot reset returned");
    }
    assert(reason == EXIT_REBOOT);
    assert(strcmp(events, "PQBSDCR") == 0);
    assert(pending_calls == 1 && start_calls == 0 && provision_calls == 0);
    assert(shutdown_calls == 0);
}

static void test_invalid_boot_intent_waits_for_key(void) {
    reset_fixture();
    pending_result = -1;
    int reason = setjmp(exit_point);
    if (reason == 0) {
        (void)mybot_run();
        assert(!"invalid-intent wait returned");
    }
    assert(reason == EXIT_IDLE);
    assert(strcmp(events, "PQ") == 0);
    assert(begin_calls == 0 && start_calls == 0 && failed_screens == 1);
    assert(shutdown_calls == 0);
}

static void test_s2_provision_preserved(void) {
    reset_fixture();
    request_provision_when_running = true;
    int reason = setjmp(exit_point);
    if (reason == 0) {
        (void)mybot_run();
        assert(!"provisioning test returned");
    }
    assert(reason == EXIT_PROVISION);
    assert(strstr(events, "NASV") != NULL);
    assert(start_calls == 1 && provision_calls == 1 && stop_calls == 1);
    assert(begin_calls == 0 && disconnect_calls == 0 && clear_calls == 0);
}

static void test_network_reset_cancel_precedes_start(void) {
    reset_fixture();
    request_reset_during_network = true;
    int reason = setjmp(exit_point);
    if (reason == 0) {
        (void)mybot_run();
        assert(!"cancelled network reset returned");
    }
    assert(reason == EXIT_REBOOT);
    assert(strstr(events, "NBSDCR") != NULL);
    assert(start_calls == 0 && clear_calls == 1 && reboot_calls == 1);
}

static void test_s2_waits_after_incomplete_stop(void) {
    for (unsigned retry = 0; retry < 2; ++retry) {
        reset_fixture();
        request_provision_when_running = true;
        stop_failures = 1;
        inject_provision_after = retry ? 2 : 0;
        int reason = setjmp(exit_point);
        if (reason == 0) {
            (void)mybot_run();
            assert(!"incomplete stop test returned");
        }
        if (retry) {
            assert(reason == EXIT_PROVISION);
            assert(stop_calls == 2 && provision_calls == 1 && recovery_waits == 2);
            assert(strstr(events, "NASSV") != NULL);
        } else {
            assert(reason == EXIT_IDLE);
            assert(stop_calls == 1 && provision_calls == 0 && recovery_waits == 4);
        }
        assert(begin_calls == 0 && clear_calls == 0 && shutdown_calls == 0);
    }
}

static void test_startup_failures_keep_keys_for_reprovision(void) {
    for (unsigned fail_start = 0; fail_start < 2; ++fail_start) {
        reset_fixture();
        network_failed = !fail_start;
        start_failed = fail_start;
        inject_provision_after = 2;
        int reason = setjmp(exit_point);
        if (reason == 0) {
            (void)mybot_run();
            assert(!"startup recovery test returned");
        }
        assert(reason == EXIT_PROVISION);
        assert(provision_calls == 1 && shutdown_calls == 0 && recovery_waits == 2);
        assert(start_calls == fail_start);
        assert(begin_calls == 0 && clear_calls == 0);
    }
}

static void test_s2_cannot_replace_failed_reset(void) {
    reset_fixture();
    clear_failures = 1;
    inject_s2_on_reset_failure = true;
    run_reset_loop();
    assert(discarded_provision == 1 && !queued_provision);
    assert(begin_calls == 1 && clear_calls == 1 && failed_screens == 1);
    assert(provision_calls == 0 && reboot_calls == 0 && idle_polls == 4);
}

int main(void) {
    test_success_order();
    test_stopped_guard_and_explicit_retry();
    test_failed_step_requires_fresh_request();
    test_delayed_new_key_retries_once();
    test_boot_resume_before_network();
    test_invalid_boot_intent_waits_for_key();
    test_s2_provision_preserved();
    test_network_reset_cancel_precedes_start();
    test_s2_waits_after_incomplete_stop();
    test_startup_failures_keep_keys_for_reprovision();
    test_s2_cannot_replace_failed_reset();
    puts("Application reset owner: durable intent, STOPPED/network gates and S2 flow passed");
    return 0;
}
