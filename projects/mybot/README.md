# MyBot Project (BK7259)

- [中文](./README_CN.md)

## 1. Overview

`mybot` is the BK7259 AP/CP firmware integration for the MyBot SDK. It combines
device-service control, bidirectional RTC audio, local provisioning and UI, and
one-way encoded video uplink in one conversation.

The SDK snapshot follows the latest committed upstream `main` revision
`58f0ef9c14be9f159b1dc167bf0f035983b25117` (tag `v1.3.0`, version 1.3.0).
This refresh changes release metadata and source comments, preserving the previous
platform interfaces, RTC and media behavior.
The target retains its HTTPS-body logging patch.
MCP control over RTM requires one `message.user` envelope, `data_type: "message"`,
and string `content` containing DeviceCommand v1 `audio.set_volume` with an integer
`params.level` from 0 to 100, or `audio.adjust_volume` with an integer `params.delta`
from -100 to 100. Positive deltas increase the volume, negative deltas decrease it,
and zero leaves it unchanged; the result is clamped to 0–100.
Sender/channel/conversation/generation checks and FIFO deduplication of the latest
16 distinct command IDs remain in effect; a duplicate ID does not apply an adjustment
again. Bare commands, mute and emotion commands are not executed over RTM;
`issued_at`/`expires_at` remain optional and ignored. Binding polls/stop notifications
run on a background service queue so local hangup completes before HTTP notification.
The pixel-glasses UI retains local expressions and voiceprint/server phases; cloud
MCP emotion input is disabled in this snapshot. AVDK, audio, keys, video, power,
GPIO and partitions retain the old-board configuration.

## 2. AP and CP Roles

| Core | Responsibility |
| --- | --- |
| AP (`bk7259_ap`) | Owns the product control loop, APSTA provisioning, MyBot SDK and AOSL lifecycle, HTTPS/RTC/RTM, audio, LCD, keys, camera capture, H.264 encoding, and video uplink. |
| CP (`bk7259`) | Initializes the BK runtime and starts the AP system. |

The `bk7259` project target builds and packages both cores.

## 3. Build

The official BK dependencies are pinned by the parent repository's gitlinks and
`vendor.lock.json`. From the `mybot-bk7259` repository root:

```bash
python3 scripts/build_all.py
```

The builder prepares isolated official-source copies, applies the recorded DNS
and camera-helper patches, then builds both AP/CP cores for each language. It
does not modify tracked source in either official submodule. To build only one
language use `--language zh-CN` or `--language en-US`; clean is the default, and
`--no-clean` requests an incremental build.

For a manual `make` build, first prepare the workspace and then run the exact
`make` command it prints, including its generated `SDK_DIR`:

```bash
python3 scripts/prepare_vendor.py --language zh-CN
```

The published complete flash images are:

```text
releases/bk7259-zh-CN-video.bin
releases/bk7259-en-US-video.bin
```

Intermediate `all-app.bin`, `app_pack.rbl`, and `build_summary.txt` are under
`build/build-bk7259-<language>-video/solution/projects/mybot/build/bk7259/mybot/package/`.
The per-core ELF files are under that generated project's
`build/bk7259/mybot/bk7259_ap/` and `build/bk7259/mybot/bk7259/`.
Product sources now live in the parent repository; edit them here rather than
inside generated workspaces. See [official dependency maintenance](../../docs/OFFICIAL_DEPENDENCIES.md).

## 4. Multimodal Video Uplink

The default AP configuration enables `CONFIG_MYBOT_VIDEO=y`. The BK7259 video
path is:

```text
MIPI CSI GC2053/CV2005 sensor (1280x720 at 5 fps)
  -> ISP MP (640x480 NV12)
  -> hardware FLEXA H.264 encoder
  -> encoded-frame pool
  -> MyBot video handler
  -> Agora RTSA high video stream
```

Current settings and behavior:

| Item | Value |
| --- | --- |
| Encoded output | H.264 Annex-B access units, 640x480 |
| Sensor input | 1280x720 at 5 fps |
| Bitrate range | 256000-512000 bit/s; initial midpoint 384000 bit/s |
| GOP | 30 frames, approximately 6 seconds at 5 fps; RTSA key-frame requests can force an IDR |
| Lifecycle | Capture and encoding start only after RTC reaches `CONNECTED`; they stop before leaving RTC |
| Direction | Device uplink only; remote video subscription is disabled |
| RTSA timing | `frame_rate=0`, so RTSA uses real frame timestamps instead of synthesizing a fixed cadence |

RTSA bandwidth estimates are clamped to the configured bitrate range and
applied to the hardware encoder. The audio path remains bidirectional while
video is uplink-only.

Video configuration is defined in:

- `components/mybot/mybot_sdk/Kconfig`
- `projects/mybot/ap/config/bk7259_ap/defconfig`

After changing video configuration, run a clean rebuild so the generated
configuration cannot retain stale values.

## 5. Display and Expressions

The JD9855 panel retains its native 320x385 geometry and logical 385x320 RGB565
surface, GPIO53 power, GPIO5 reset and active-low GPIO7 backlight. Its LVGL view
keeps terminal text, pairing codes and the main glasses artwork inside the
visible area of the panel's pronounced rounded corners; their physical radius
has not been measured precisely. The view
uses the simulator's process-gray skin and pixel glasses, with four terminal
rows for mood, runtime state and conversation phase/voiceprint text. Startup,
provisioning, web-console pairing and errors retain bilingual guidance;
factory reset shows restoring, retry and reboot guidance above ordinary SDK updates.
S3 retains its configured ADC long press; reset intent survives power loss, and cleanup requires
SDK/network shutdown plus verified deletion of only the three MyBot records. Failed network/SDK
startup keeps keys and LCD available for S2 reprovisioning or S3 reset. The artwork is 346x302 at (19,9),
with 9-pixel terminal rows and a 10-pixel signature. Pairing codes are exactly
six digits and use a fixed 32-pixel size. The legacy light-theme option is retained for saved configurations;
the pixel renderer always uses the fixed process-gray appearance.

| Runtime state | Face and timing |
| --- | --- |
| READY / hangup | NEUTRAL |
| Enter conversation | SURPRISED, 300 ms hold plus entering transition |
| LISTENING | LISTENING |
| THINKING | THINKING; PROCESSING after three continuous seconds |
| SPEAKING / no phase | NEUTRAL |

With animations enabled, the conversation opener adds a 250 ms entering transition
before its hold. Ordinary state and voiceprint updates do not restart that hold.
Cloud MCP emotion input is disabled in the current SDK snapshot. Opener expiry
restores the latest automatic phase face, including PROCESSING if thinking has continued
for three seconds in the background. Leaving a conversation clears its timers;
operating guidance and factory reset take precedence. Expressions control LCD
artwork only and do not send motor commands.

The 23 expression names are:

```text
NEUTRAL, GREETING, HAPPY, EXCITED, CURIOUS, CONFUSED, THINKING, LISTENING,
SURPRISED, SHY, PROUD, LOVE, SLEEPY, BORED, SAD, PLEADING, APOLOGY, WORRIED,
ANNOYED, REFUSE, PROCESSING, ERROR, SLEEP
```

`include/bk7259_expression.h` provides name conversion and copied local display
requests via `bk7259_lcd_set_expression()` / `bk7259_lcd_clear_expression()`.
Local overrides persist until cleared or a workflow change; repeating a local
override does not replay it. Clearing restores the latest automatic face. ERROR
and SLEEP artwork do not change runtime state.
The existing display owner performs rendering, animations and expiration;
no additional UI thread or UTC clock is required.

## 6. Real-Device Validation

- [ ] Cold boot the combined `all-app.bin` and confirm both AP and CP start.
- [ ] Confirm the installed GC2053 or CV2005 camera is detected and the source reports 1280x720 at 5 fps with 640x480 H.264 output.
- [ ] Start a conversation and verify audio in both directions plus H.264 video at the remote endpoint.
- [ ] Verify camera capture and encoding start only after RTC is connected and emit no frames after conversation teardown begins.
- [ ] Measure approximately 5 fps, a 256-512 kbit/s encoder rate, and the initial 384 kbit/s target.
- [ ] Verify a periodic IDR about every 6 seconds and that an RTSA key-frame request forces a new IDR.
- [ ] Exercise constrained Wi-Fi and confirm RTSA bitrate updates stay within 256-512 kbit/s without breaking audio.
- [ ] End, restart, and interrupt conversations repeatedly; confirm the encoder, frame pool, camera, and RTC resources stop and restart cleanly.
- [ ] Check all 23 faces, four terminal rows, both languages and pairing guidance on the 385x320 logical display; terminal text, pairing codes and the main glasses artwork must remain visible inside all four rounded corners.
- [ ] Verify the SURPRISED opener, phase transitions and three-second THINKING timeout; hangup must restore NEUTRAL.
- [ ] Test MCP `audio.set_volume` at 0, 50 and 100 through the `message.user` envelope; bare commands, mute and emotion commands must have no effect.
- [ ] Test wrapped `audio.adjust_volume` with positive, negative and zero deltas, including -100 and 100; results must clamp to 0–100, and repeating a command ID must not adjust the volume again.
- [ ] Verify local expression set/clear and repeated overrides without motor output; repeated phase/voiceprint updates must preserve the conversation opener timing.
- [ ] Regress the original four ADC keys, long-press provisioning/factory reset, audio and video while exercising UI animations.
