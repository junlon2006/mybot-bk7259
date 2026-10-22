# MyBot Project (BK7259)

- [简体中文](./README_CN.md)
- [Repository overview](../../README.md)
- [Validation status](./UI_VALIDATION.md)

## Scope

This directory contains the BK7259 product integration for the MyBot SDK on the
Robot V2 reference board. It owns board initialization, APSTA/STA networking, local
peripherals, product policy, and the MyBot lifecycle boundary.

The MyBot SDK owns device-service registration, authentication, pairing,
conversation state, RTC/RTM, and voiceprint handling. Cloud services and AI agents
are external dependencies.

## AP and CP

| Core | Responsibility |
| --- | --- |
| CP (`bk7259`) | Initializes the BK runtime and starts the AP system. |
| AP (`bk7259_ap`) | Runs the product task, networking, MyBot lifecycle, audio, display, keys, camera, and H.264 encoding. |

The `bk7259` project target builds and packages both cores.

## Startup and Network Ownership

The AP entry point is [ap_main.c](./ap/ap_main.c):

1. Initialize the BK runtime and media service.
2. Create the `mybot_app` product task.
3. Derive the device ID from the chip UID and build runtime configuration.
4. Initialize AOSL and prepare the product-owned display and key manager.
5. Resume a pending factory reset, restore saved Wi-Fi, or enter APSTA provisioning.
6. Start MyBot after STA IPv4 connectivity is available.
7. Poll SDK state and process conversation, provisioning, and reset requests.

Wi-Fi is owned by the product layer. The SDK adapter only forwards serialized
connectivity transitions. Reprovisioning waits for `mybot_stop()` and requires both
`STOPPED` and `mybot_is_running() == false` before starting APSTA.

Network or SDK failures keep the product display and keys available for S2
reprovisioning or S3 factory reset.

## ADC Keys and Factory Reset

Robot V2 uses two resistor ladders on ADC pads. A key is identified by its ADC
channel and voltage window:

| Key | ADC | Window | Short press | Long press |
| --- | --- | ---: | --- | --- |
| S1 | CEN | — | Hardware reset | — |
| S2 | 15 / GPIO13 | 500-1500 mV | Start or stop conversation | Reprovision |
| S3 | 15 / GPIO13 | 4500-6000 mV | — | Factory reset |
| S4 | 4 / GPIO28 | 500-1500 mV | Volume down | — |
| S5 | 4 / GPIO28 | 4500-6000 mV | Volume up | — |

`CONFIG_ADC_KEY_LONG_PRESS_MS` is 2000 ms. The ADC pads remain high-Z without
internal pulls so the divider voltage is not distorted. GPIO8 and GPIO9 are the
32.768 kHz crystal pins and remain high-Z.

Factory reset is owned by the product task:

1. Stop MyBot and require `STOPPED`.
2. Stop the portal, scan, and STA.
3. Delete and verify `mybot.device_auth`, `mybot.wifi.v1`, and `mybot.volume.v1`.
4. Remove the reset marker and reboot.

The reset does not erase unrelated EasyFlash records or change the UID-derived
device ID. An interrupted reset resumes before network or SDK startup. A failed
cleanup keeps the intent and waits for another explicit S3 long press.

## Audio

MyBot uses 16 kHz, mono, signed 16-bit PCM. The Robot V2 capture path uses dual
DMIC inputs and on-device hardware AEC. Speaker playback uses the onboard DAC path.

Volume changes are routed through the SDK device-volume adapter and persisted in
EasyFlash. The board exposes volume up/down through S5/S4 ADC keys. Capture,
playback, AEC, and repeated conversation cycling still require target validation.

## Display

The product owns the JD9855 MIPI panel from platform preparation through shutdown,
so provisioning remains visible while MyBot is stopped. The native 320x385 panel
is rendered as a 385x320 logical RGB565 surface.

Board resources:

- GPIO53: panel power
- GPIO5: panel reset
- GPIO7: active-low backlight

The LVGL 9.5.0 view uses the process-gray pixel-glasses design with four terminal
rows. It shows provisioning, pairing, ready, conversation phase, voiceprint, error,
and factory-reset states with Chinese or English guidance.

The view reserves a conservative safe area for the panel's rounded corners. The
hardware radius is not measured precisely. GPU and touch are disabled. Display
resource provenance is recorded in
[display/SOURCES.md](../../components/mybot/mybot_sdk/platforms/bk7259/display/SOURCES.md).

## Video

Video is enabled in the default AP configuration. The uplink path is:

```text
GC2053/CV2005 MIPI CSI sensor, 1280x720 at 5 fps
  -> ISP MP scaling to 640x480 NV12
  -> hardware FLEXA H.264 encoder
  -> complete Annex-B access units
  -> MyBot video handler
  -> Agora RTC uplink
```

The encoder bitrate range is 256000-512000 bit/s with a 384000 bit/s initial target.
Video starts only after RTC reaches `CONNECTED` and stops before RTC leaves. The
device does not receive or render remote video.

Configuration and source are in
[defconfig](./ap/config/bk7259_ap/defconfig) and
[bk7259_video.c](../../components/mybot/mybot_sdk/platforms/bk7259/bk7259_video.c).

## Validation

Host tests live under [`tests/`](./tests/). Current status and remaining target
checks are in [UI_VALIDATION.md](./UI_VALIDATION.md).

The tests cover expression policy, HTTPS transport behavior, factory-reset storage,
reset ownership, and Wi-Fi/portal faults. Host tests do not validate ADC voltages,
DMIC/AEC quality, display timing, camera behavior, power loss, or long-duration
audio/video operation.
