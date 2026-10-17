/* SPDX-License-Identifier: Apache-2.0 */
#include "test_env.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Compile the production manager with fault-injected vendor APIs. */
#include "bk7259_wifi.c"

static int portal_calls;
int bk7259_wifi_portal_run(const char *device_id) {
  (void)device_id;
  ++portal_calls;
  return -1;
}

static void begin_test(void) {
  test_env_reset();
  portal_calls = 0;
  assert(bk7259_wifi_prepare() == 0);
}

static void finish_test(void) {
  test_env.reset = false;
  test_env.sta_stop_result = 0;
  test_env.scan_stop_result = 0;
  bk7259_wifi_shutdown();
  assert(!s_wifi.prepared && !s_wifi.lock && !s_wifi.operation_lock);
}

static void set_connected(void) {
  test_env.connected = true;
  strcpy(test_env.connected_ssid, "test-network");
  strcpy(s_wifi.target_ssid, "test-network");
  s_wifi.sta_started = true;
  ++s_wifi.sta_generation;
}

static void driver_connect(void) {
  test_env.connected = true;
  strcpy(test_env.connected_ssid, "test-network");
}

static void test_sta_stop_failure_is_retryable(void) {
  begin_test();
  set_connected();
  test_env.sta_stop_result = -1;
  assert(bk7259_wifi_manager_disconnect() == -1);
  assert(s_wifi.sta_started && test_env.sta_stops == 1);
  assert(bk7259_wifi_manager_connect_candidate("other", "secret-password", 1) ==
         -1);
  assert(test_env.sta_starts == 0 && test_env.sta_stops == 2);
  test_env.sta_stop_result = 0;
  assert(bk7259_wifi_manager_disconnect() == 0);
  assert(!s_wifi.sta_started && test_env.sta_stops == 3);
  finish_test();
}

static void test_partial_sta_start_retains_failed_cleanup(void) {
  begin_test();
  test_env.sta_start_result = -1;
  test_env.sta_stop_result = -1;
  assert(bk7259_wifi_manager_connect_candidate("test-network",
                                               "secret-password", 1) == -1);
  assert(s_wifi.sta_started && test_env.sta_starts == 1 &&
         test_env.sta_stops == 1);
  test_env.sta_stop_result = 0;
  assert(bk7259_wifi_manager_disconnect() == 0);
  assert(!s_wifi.sta_started && test_env.sta_stops == 2);
  finish_test();
}

static void test_scan_stop_failure_is_retryable(void) {
  begin_test();
  wifi_scan_ap_info_t result[1];
  size_t count = 99;
  test_env.scan_stop_result = -1;
  assert(bk7259_wifi_manager_scan_sync(result, 1, &count, 0) == -1);
  assert(count == 0 && s_wifi.scan_in_progress && test_env.scan_stops == 1);
  assert(bk7259_wifi_manager_scan_sync(result, 1, &count, 0) == -1);
  assert(test_env.scan_starts == 1 && test_env.scan_stops == 2);
  test_env.scan_stop_result = 0;
  assert(bk7259_wifi_manager_disconnect() == 0);
  assert(!s_wifi.scan_in_progress && test_env.scan_stops == 3);
  finish_test();
}

static void test_shutdown_retains_failed_network_cleanup(void) {
  begin_test();
  set_connected();
  test_env.sta_stop_result = -1;
  bk7259_wifi_shutdown();
  assert(s_wifi.prepared && s_wifi.shutting_down && s_wifi.sta_started &&
         s_wifi.lock);
  assert(bk7259_wifi_prepare() == -1);
  test_env.sta_stop_result = 0;
  bk7259_wifi_shutdown();
  assert(!s_wifi.prepared && test_env.sta_stops == 2);
}

static void test_pending_reset_prevents_new_work(void) {
  begin_test();
  test_env.reset = true;
  wifi_scan_ap_info_t result[1];
  size_t count = 99;
  assert(bk7259_wifi_manager_connect_candidate("test-network",
                                               "secret-password", 1) == -2);
  assert(bk7259_wifi_manager_scan_sync(result, 1, &count, 1) == -2 &&
         count == 0);
  assert(bk7259_wifi_manager_save_credentials("test-network",
                                              "secret-password") == -2);
  assert(mybot_bk7259_ensure_network("device") == -2);
  assert(mybot_bk7259_provision_wifi("device") == -2);
  assert(test_env.sta_starts == 0 && test_env.scan_starts == 0 &&
         test_env.saves == 0);
  assert(portal_calls == 0 && test_env.reset);
  finish_test();
}

static void test_reset_during_waits_and_flash(void) {
  begin_test();
  test_env.on_delay = test_request_reset;
  assert(bk7259_wifi_manager_connect_candidate("test-network",
                                               "secret-password", 1) == -2);
  assert(test_env.sta_starts == 0);
  finish_test();

  begin_test();
  test_env.on_sta_start = test_request_reset;
  assert(bk7259_wifi_manager_connect_candidate("test-network",
                                               "secret-password", 1) == -2);
  assert(test_env.sta_starts == 1 && test_env.sta_stops == 1 &&
         !s_wifi.sta_started);
  finish_test();

  begin_test();
  test_env.on_scan_start = test_request_reset;
  wifi_scan_ap_info_t ap[1];
  size_t count;
  assert(bk7259_wifi_manager_scan_sync(ap, 1, &count, 1000) == -2);
  assert(test_env.scan_stops == 1 && !s_wifi.scan_in_progress);
  finish_test();

  begin_test();
  set_connected();
  test_env.on_link_status = test_request_reset;
  assert(bk7259_wifi_manager_save_credentials("test-network",
                                              "secret-password") == -2);
  assert(test_env.saves == 0);
  finish_test();

  begin_test();
  set_connected();
  test_env.on_save = test_request_reset;
  assert(bk7259_wifi_manager_save_credentials("test-network",
                                              "secret-password") == -2);
  assert(test_env.saves == 1 && test_env.reset);
  finish_test();
}

static mybot_wifi_event_t received_events[8];
static size_t event_count;
static void sdk_event(mybot_wifi_event_t event, void *user) {
  assert(user == &event_count && event_count < 8);
  received_events[event_count++] = event;
}

static void test_runtime_reconnect_and_detach_order(void) {
  begin_test();
  set_connected();
  s_wifi.sta_has_ip = true;
  event_count = 0;
  void *ctx = NULL;
  assert(g_mybot_bk7259_wifi_ops.init(&ctx, "device", sdk_event,
                                      &event_count) == 0);
  assert(event_count == 1 &&
         received_events[0] == MYBOT_WIFI_EVENT_STA_CONNECTED);
  wifi_event_callback(&s_wifi, EVENT_MOD_WIFI, EVENT_WIFI_STA_DISCONNECTED,
                      NULL);
  wifi_event_callback(&s_wifi, EVENT_MOD_WIFI, EVENT_WIFI_STA_DISCONNECTED,
                      NULL);
  netif_event_got_ip4_t ip = {.netif_if = NETIF_IF_STA};
  wifi_event_callback(&s_wifi, EVENT_MOD_NETIF, EVENT_NETIF_GOT_IP4, &ip);
  wifi_event_callback(&s_wifi, EVENT_MOD_NETIF, EVENT_NETIF_GOT_IP4, &ip);
  assert(event_count == 3 &&
         received_events[1] == MYBOT_WIFI_EVENT_STA_DISCONNECTED);
  assert(received_events[2] == MYBOT_WIFI_EVENT_STA_CONNECTED);
  g_mybot_bk7259_wifi_ops.destroy(ctx);
  wifi_event_callback(&s_wifi, EVENT_MOD_WIFI, EVENT_WIFI_STA_DISCONNECTED,
                      NULL);
  assert(event_count == 3 && s_wifi.sta_started && test_env.sta_stops == 0);
  finish_test();
}

static void
test_reprovision_preserves_other_records_and_reconnect_policy(void) {
  begin_test();
  test_env.on_sta_start = driver_connect;
  assert(bk7259_wifi_manager_connect_candidate("test-network",
                                               "secret-password", 1) == 0);
  assert(!test_env.sta_config.disable_auto_reconnect_after_disconnect);
  assert(test_env.sta_config.auto_reconnect_count == 0);
  assert(test_env.sta_config.auto_reconnect_timeout == 0);
  assert(mybot_bk7259_provision_wifi("device") == -1);
  assert(portal_calls == 1 && test_env.saves == 0);
  finish_test();
}

int main(void) {
  test_sta_stop_failure_is_retryable();
  test_partial_sta_start_retains_failed_cleanup();
  test_scan_stop_failure_is_retryable();
  test_shutdown_retains_failed_network_cleanup();
  test_pending_reset_prevents_new_work();
  test_reset_during_waits_and_flash();
  test_runtime_reconnect_and_detach_order();
  test_reprovision_preserves_other_records_and_reconnect_policy();
  puts("Wi-Fi manager: fault cleanup, cancellation, reconnect and detach "
       "checks passed");
  return 0;
}
