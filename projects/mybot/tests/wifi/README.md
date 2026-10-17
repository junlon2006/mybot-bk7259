# Wi-Fi lifecycle host checks

Run `bash projects/mybot/tests/wifi/run.sh` from the repository root. Set
`SANITIZE=1` to enable AddressSanitizer and UndefinedBehaviorSanitizer.

The harness compiles the actual product manager and HTTP portal against fault-injected
vendor APIs and the pinned AVDK JSON parser. It covers failed STA, scan and SoftAP
stops followed by retries; partial start failures; cancellation before and during
connection, scanning, credential writes and HTTP I/O; socket/buffer cleanup; and SDK
disconnect/reconnect ordering and detach. It also checks that standalone Wi-Fi
reprovisioning retains its entry point and STA auto-reconnect settings, and that
provisioning logs never expose the submitted password.

These deterministic host checks validate software ownership and cancellation paths.
They do not replace on-device radio, RTOS scheduling, flash power-loss or network tests.
