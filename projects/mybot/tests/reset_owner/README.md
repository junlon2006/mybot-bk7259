# Application reset-owner regression tests

```sh
bash projects/mybot/tests/reset_owner/run.sh
ASAN_OPTIONS=detect_leaks=0 RESET_OWNER_TEST_SANITIZERS=1 \
    bash projects/mybot/tests/reset_owner/run.sh
```

The harness includes the actual `projects/mybot/ap/ap_main.c` and invokes its
reset loop and application entry flow. It uses the real public SDK lifecycle
declarations. Mocks provide only external SDK/platform services, vendor APIs,
and the generated version header; no replacement reset coordinator is present.
`setjmp` terminates expected idle/reboot paths so the real infinite owner loops
remain unchanged.

Coverage includes durable intent before SDK stop; both running and STOPPED-state
guards before deleting records; network teardown failure preventing deletion;
failed intent/delete/stop requiring a fresh reset request; interrupted-reset
boot resume before networking; corrupt boot intent waiting for a new request;
network cancellation entering reset before SDK start; and S2 remaining a separate
reprovisioning action that cannot replace a reset already in progress.

Network and SDK-startup failure paths retain platform/key ownership. An incomplete
SDK stop prevents APSTA provisioning until a new S2 request retries stop and
reaches STOPPED. The companion `factory_reset` suite tests the production storage
module and EasyFlash fault cases separately.

This suite does not validate physical ADC thresholds/debounce, panel scanout,
hardware reboot, or actual SDK worker termination. Those still require firmware
and hardware regression testing.
