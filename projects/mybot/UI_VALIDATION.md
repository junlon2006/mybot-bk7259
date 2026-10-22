# BK7259 Firmware Validation

**English | [简体中文](UI_VALIDATION.zh-CN.md)**

## Scope

This page records the current reproducible checks and remaining release blockers
for the `main` Robot V2 firmware. It replaces the former chronological development
log. Older intermediate diagnostics remain available in Git history.

The status below was reviewed on 2026-10-22. No target-board test or full firmware
rebuild was performed during this documentation update.

## Current Source and Host Checks

The pinned source set passes:

```bash
python3 scripts/check_sources.py
```

This verifies:

- AVDK and Solution gitlinks and tracked-tree cleanliness.
- Patch hashes and the MyBot/AOSL content digests.
- RTSA header and static-library hashes.
- Product include boundaries.

Build-preparation tests:

```bash
(cd scripts && python3 -m unittest test_build_all.py test_vendor_workspace.py)
```

Current result: 16 tests pass.

Product host tests:

```bash
for script in projects/mybot/tests/*/run.sh; do
  bash "$script"
done
```

The suites cover:

- Expression and LCD indicator policy: 13 groups and 462 assertions.
- HTTPS retry, timeout, EOF, socket-error isolation, and clock wraparound.
- Factory-reset persistence, boot resume, deletion, and network faults.
- Reset ownership, SDK/network stop gates, and S2 reprovisioning.
- Wi-Fi manager cleanup, cancellation, reconnect, detach, portal, and storage faults.

All current suites pass. These are host tests and do not validate target hardware.

## Firmware Build

The firmware rebuild command is:

```bash
python3 scripts/build_all.py
```

It builds Chinese and English AP/CP images and verifies the generated service
endpoint. The documentation update did not rerun the firmware build, so this page
does not claim a new build-size or timing measurement.

## Release Blockers

These items require code or contract changes:

1. **Credential logging:** the pinned MyBot HTTPS debug patch logs complete request
   and response bodies at `NOTICE`. Responses may contain device, pairing, or RTC
   tokens. Remove or redact this logging before production.
2. **Certificate dates:** the adapter requests certificate and hostname
   verification, but the active MbedTLS configuration does not enable
   `MBEDTLS_HAVE_TIME_DATE`. Certificate expiry and not-before checks must be
   restored and tested.
3. **Credential storage:** Wi-Fi and device credentials are stored in EasyFlash
   without at-rest encryption.
4. **RTSA distribution:** the repository contains the proprietary RTSA input, but
   no standalone redistribution grant is recorded.
5. **Target measurements:** firmware Flash, OTA size, PSRAM peaks, and stack high-water
   marks must be re-measured after the final code and asset set is selected.

## Target Board Checks

Before production:

- Cold boot the combined `all-app.bin` and verify both cores.
- Validate Robot V2 ADC key windows and all short/long press functions.
- Validate dual-DMIC capture, hardware AEC, volume keys, and bidirectional RTC audio.
- Validate provisioning, cancellation, reconnect, and power-loss recovery.
- Validate camera detection, H.264 output, frame pacing, bitrate changes, IDR, and
  repeated video sessions.
- Validate JD9855 orientation, colors, rounded-corner safe area, both languages, and
  all 23 expressions.
- Measure stack, heap, PSRAM, and timing headroom with audio, video, UI, HTTP, and RTC
  active together.
- Validate the deployed certificate chain, hostname, expiry, and timeout behavior.
- Run long-duration and watchdog-recovery tests.

Host tests and firmware compilation are not evidence of target hardware behavior.
