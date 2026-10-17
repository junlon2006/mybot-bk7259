/* SPDX-License-Identifier: Apache-2.0 */
#include "test_env.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Exercise the production parser, HTTP loop and resource cleanup. */
#include "bk7259_wifi_portal.c"

static int candidate_calls, save_calls, disconnect_calls, scan_calls;
static void (*on_candidate)(void);
static void (*on_save_credentials)(void);
int bk7259_wifi_manager_connect_candidate(const char *ssid,
                                          const char *password,
                                          uint32_t timeout) {
  assert(strcmp(ssid, "test-network") == 0);
  assert(strcmp(password, "secret-password") == 0 && timeout > 0);
  ++candidate_calls;
  if (on_candidate)
    on_candidate();
  return 0;
}
int bk7259_wifi_manager_save_credentials(const char *ssid,
                                         const char *password) {
  assert(strcmp(ssid, "test-network") == 0 &&
         strcmp(password, "secret-password") == 0);
  ++save_calls;
  if (on_save_credentials)
    on_save_credentials();
  return 0;
}
int bk7259_wifi_manager_disconnect(void) {
  ++disconnect_calls;
  return 0;
}
int bk7259_wifi_manager_scan_sync(wifi_scan_ap_info_t *results, size_t capacity,
                                  size_t *count, uint32_t timeout) {
  (void)results;
  (void)capacity;
  (void)timeout;
  ++scan_calls;
  *count = 0;
  return test_env.reset ? BK7259_NETWORK_CANCELLED : 0;
}

static void begin_test(void) {
  assert(!s_softap_started);
  test_env_reset();
  candidate_calls = save_calls = disconnect_calls = scan_calls = 0;
  on_candidate = on_save_credentials = NULL;
}

static portal_http_request_t submit_request(void) {
  portal_http_request_t request = {
      .has_content_length = true,
      .content_type_json = true,
  };
  strcpy(request.body,
         "{\"ssid\":\"test-network\",\"password\":\"secret-password\"}");
  request.body_length = strlen(request.body);
  return request;
}

static void test_softap_stop_failure_is_retryable(void) {
  begin_test();
  portal_context_t context = {.ap_ssid = "mybot-test"};
  assert(start_softap(&context) == 0);
  test_env.ap_stop_result = -1;
  assert(bk7259_wifi_portal_stop() == -1 && s_softap_started);
  assert(bk7259_wifi_portal_run("device") == -1);
  assert(test_env.ap_starts == 1 && test_env.ap_stops == 2);
  test_env.ap_stop_result = 0;
  assert(bk7259_wifi_portal_stop() == 0 && !s_softap_started);
  assert(test_env.ap_stops == 3);
}

static void test_partial_start_cleanup_can_retry(void) {
  begin_test();
  test_env.ap_start_result = -1;
  test_env.ap_stop_result = -1;
  assert(bk7259_wifi_portal_run("device") == -1);
  assert(s_softap_started && test_env.ap_starts == 1 && test_env.ap_stops == 1);
  test_env.ap_stop_result = 0;
  assert(bk7259_wifi_portal_stop() == 0 && !s_softap_started);
  assert(test_env.ap_stops == 2);
}

static void test_cancel_before_softap_or_submit(void) {
  begin_test();
  test_env.reset = true;
  portal_http_request_t request = submit_request();
  assert(bk7259_wifi_portal_run("device") == BK7259_NETWORK_CANCELLED);
  assert(handle_submit(6, &request) == BK7259_NETWORK_CANCELLED);
  assert(test_env.ap_starts == 0 && candidate_calls == 0 && save_calls == 0);
  assert(test_env.socket_writes == 0 && test_env.reset);
}

static void test_cancel_during_submit(void) {
  begin_test();
  portal_http_request_t request = submit_request();
  on_candidate = test_request_reset;
  assert(handle_submit(6, &request) == BK7259_NETWORK_CANCELLED);
  assert(candidate_calls == 1 && save_calls == 0 &&
         test_env.socket_writes == 0);

  begin_test();
  on_save_credentials = test_request_reset;
  assert(handle_submit(6, &request) == BK7259_NETWORK_CANCELLED);
  assert(candidate_calls == 1 && save_calls == 1 &&
         test_env.socket_writes == 0);
}

static void test_cancel_during_send_and_receive(void) {
  begin_test();
  test_env.on_select = test_request_reset;
  assert(send_http_response(6, "200 OK", "text/plain", NULL, "OK", 2) == -2);
  assert(test_env.socket_writes == 0);

  begin_test();
  test_env.on_send = test_request_reset;
  assert(send_http_response(6, "200 OK", "text/plain", NULL, "OK", 2) == -2);
  assert(test_env.socket_writes == 1);

  begin_test();
  portal_http_request_t request = {0};
  test_env.on_select = test_request_reset;
  assert(receive_http_request(6, &request) == -2);
  assert(test_env.socket_reads == 0 && test_env.allocations == 0);
}

static void reset_during_partial_request(void) {
  if (test_env.selects >= 3)
    test_request_reset();
}

static void test_portal_cancel_closes_fds_and_retries_softap(void) {
  begin_test();
  test_env.http_request = "POST /submit HTTP/1.1\r\nContent-Length: 51\r\n";
  test_env.on_select = reset_during_partial_request;
  test_env.ap_stop_result = -1;
  assert(bk7259_wifi_portal_run("device") == -2);
  assert(test_env.socket_closes == 2 && test_env.allocations == 0);
  assert(test_env.listener_timeout_ms == 200);
  assert(s_softap_started && test_env.ap_stops == 1 && disconnect_calls == 1);
  assert(candidate_calls == 0 && save_calls == 0 && test_env.reset);
  test_env.ap_stop_result = 0;
  assert(bk7259_wifi_portal_stop() == 0 && !s_softap_started);
}

static void test_cancel_during_ap_start_releases_resources(void) {
  begin_test();
  test_env.on_ap_start = test_request_reset;
  assert(bk7259_wifi_portal_run("device") == -2);
  assert(test_env.ap_starts == 1 && test_env.ap_stops == 1 &&
         !s_softap_started);
}

static void test_successful_http_submit(void) {
  begin_test();
  portal_http_request_t request = submit_request();
  char http_request[768];
  snprintf(http_request, sizeof(http_request),
           "POST /submit HTTP/1.1\r\nContent-Type: application/json\r\n"
           "Content-Length: %zu\r\n\r\n%s",
           request.body_length, request.body);
  test_env.http_request = http_request;
  assert(bk7259_wifi_portal_run("device") == 0);
  assert(candidate_calls == 1 && save_calls == 1 &&
         test_env.socket_writes == 2);
  assert(test_env.socket_closes == 2 && !s_softap_started &&
         test_env.allocations == 0);
  assert(test_env.listener_timeout_ms == 200);
}

int main(void) {
  test_softap_stop_failure_is_retryable();
  test_partial_start_cleanup_can_retry();
  test_cancel_before_softap_or_submit();
  test_cancel_during_submit();
  test_cancel_during_send_and_receive();
  test_portal_cancel_closes_fds_and_retries_softap();
  test_cancel_during_ap_start_releases_resources();
  test_successful_http_submit();
  puts("Wi-Fi portal: HTTP cancellation, persistence and SoftAP retry checks "
       "passed");
  return 0;
}
