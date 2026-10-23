# mybot-bk7259

[![License](https://img.shields.io/github/license/junlon2006/mybot-bk7259)](LICENSE)
[![Upstream](https://img.shields.io/badge/upstream-mybot-0969da)](https://github.com/junlon2006/mybot)
[![Platform](https://img.shields.io/badge/platform-BK7259-2f6f4e)](https://github.com/bekencorp/bk_avdk_smp)

**[简体中文](README.zh-CN.md) | English**

`mybot-bk7259` is a reference firmware integration of the
[MyBot SDK](https://github.com/junlon2006/mybot) for the Beken BK7259 AP/CP
platform. The `main` branch targets the Robot V2 reference board and provides
board initialization, network ownership, peripheral adapters, firmware build
files, and local product policy. The MyBot SDK owns device-service registration,
authentication, pairing, conversation state, RTC/RTM, and voiceprint behavior.

This is a reference implementation, not a production-ready product image. Board
adaptation, security review, target testing, and third-party licensing review are
required before deployment.

## Features

- BK7259 AP/CP startup, Flash partitions, and SRAM/PSRAM layout.
- Product-owned STA reconnection and APSTA Wi-Fi provisioning with a captive portal.
- Dual-DMIC capture with on-device hardware AEC and speaker playback.
- Optional MIPI CSI camera capture, hardware H.264 encoding, and Agora RTC video uplink.
- Resistor-ladder ADC keys, JD9855 MIPI display, EasyFlash storage, HTTPS, and derived
  device identity.
- Embedded Chinese and English prompt assets.
- Managed builds that produce Chinese and English AP/CP images, a combined flash image,
  and an OTA package.

The device service and cloud AI agent are external dependencies and are not part of
this repository.

## Architecture

| Component | Responsibility |
| --- | --- |
| CP (`bk7259`) | Initializes the BK runtime and starts the AP system. |
| AP (`bk7259_ap`) | Runs the product control loop, networking, MyBot lifecycle, audio, display, keys, camera, and H.264 encoding. |
| MyBot SDK | Owns device-service registration, authentication, pairing, conversation state, RTC/RTM, and voiceprint handling. |
| BK7259 platform adapters | Implement MyBot platform `ops` using BK components and board resources. |
| AOSL | Provides the portable runtime used by MyBot and RTSA. |

The product control loop is [ap_main.c](projects/mybot/ap/ap_main.c). It derives the
device ID from the chip UID, prepares product hardware, restores or provisions Wi-Fi,
starts MyBot after STA IPv4 connectivity, and owns factory reset and product-level
recovery.

BK platform sources access MyBot through public platform headers under
`<mybot/platform/...>`. The AP application entry point owns the public MyBot lifecycle
calls.

## Repository Layout

| Path | Contents |
| --- | --- |
| `bk_avdk_smp/` | Pinned BK7259 AP/CP SDK submodule |
| `bk_solution_ai/` | Pinned BK solution submodule |
| `components/mybot/mybot_sdk/` | MyBot SDK snapshot and BK7259 platform adapters |
| `components/mybot/mybot_aosl/` | Vendored AOSL runtime |
| `components/mybot/mybot_rtsa/` | Agora RTSA headers, package metadata, and target library |
| `projects/mybot/` | AP/CP entry points, board configuration, partitions, assets, and tests |
| `patches/` | Reviewed vendor patches applied only to generated build copies |
| `scripts/` | Dependency preparation, source verification, and build helpers |
| `docs/` | Dependency and validation documentation |

Generated directories such as `build/` and `releases/` are ignored. Local reference
material is not a build input.

## Pinned Dependencies

| Dependency | Revision |
| --- | --- |
| `bk_avdk_smp` | `584b947920c414230f9909375f008e7d6a2c6c6b` |
| `bk_solution_ai` | `03aadb49756a5109bfede04002b9b6820f407c3f` |
| MyBot SDK snapshot | `baf41bac3b6ca25bad9820f9fc857c94698607d8`, version 1.3.0 |
| AOSL base | `84e086084ebcd0ae2455a0ce5721950c5fe2e656` |
| Agora RTSA | v1.10.1 build `1278380` |

The parent repository pins submodule gitlinks in `vendor.lock.json`, records the
MyBot source commit and digests in `SDK_REVISION`, records AOSL modifications in
`AOSL_REVISION`, and records the RTSA package in
`components/mybot/mybot_rtsa/PACKAGE_INFO`. Vendor patches are applied to
isolated build copies; the official checkout remains unchanged.

See [BK dependencies and patches](docs/OFFICIAL_DEPENDENCIES.md) for source roles,
patch provenance, and update rules.

## Requirements

- Linux with Git submodule support.
- Python 3.9 or later and the build dependencies required by the pinned `bk_avdk_smp`,
  including `click_option_group`.
- The BK7259 Arm GNU toolchain selected by `bk_avdk_smp`.
- A BK7259 board wired as Robot V2, or equivalent hardware.
- A compatible MyBot device service and an Agora-enabled deployment.

## Get the Source

```bash
git clone --recurse-submodules https://github.com/junlon2006/mybot-bk7259.git
cd mybot-bk7259
```

For an existing clone:

```bash
git submodule sync --recursive
git submodule update --init --recursive
git status
git submodule status
```

Use the revisions pinned by the parent repository. Do not use
`git submodule update --remote` to follow moving branch heads.

## Build

From the repository root:

```bash
python3 scripts/check_sources.py
python3 scripts/build_all.py
```

`check_sources.py` verifies the submodule revisions and cleanliness, patch hashes,
MyBot/AOSL content digests, RTSA artifact hashes, and product include boundaries.
`build_all.py` prepares isolated vendor workspaces, builds both cores for each requested
language, verifies generated configuration and endpoints, and publishes outputs only
after all requested variants succeed.

Useful options:

```bash
python3 scripts/build_all.py --language zh-CN
python3 scripts/build_all.py --language en-US
python3 scripts/build_all.py --dry-run
python3 scripts/build_all.py --no-clean
```

The pinned RTSA package supports only `MYBOT_AUDIO_PTIME_MS=60`.

### Outputs

The default outputs are:

```text
releases/bk7259-zh-CN-video.bin
releases/bk7259-en-US-video.bin
```

Generated artifacts for the Chinese build are under:

```text
build/build-bk7259-zh-CN-video/solution/projects/mybot/build/bk7259/mybot/package/
```

| File | Purpose |
| --- | --- |
| `all-app.bin` | Combined bootloader, CP, and AP flash image |
| `app_pack.rbl` | OTA package containing the CP and AP images |
| `build_summary.txt` | Flash and RAM usage report |

The CP and AP images are under `bk7259/app.bin` and `bk7259_ap/app.bin` in the
generated build tree.

For a manual vendor build, first prepare a generated project:

```bash
python3 scripts/prepare_vendor.py --language zh-CN
```

Then run the complete `make` command printed by the script. Do not build against an
unpatched vendor checkout.

## Flash and Use

Flash `all-app.bin` with the Beken flashing tool. Board-specific instructions are
provided with `bk_avdk_smp`.

Typical flow:

1. With no saved Wi-Fi credentials, the device starts an open `mybot-xxxx` SoftAP.
2. Connect to it, open `http://192.168.4.1/`, and submit the target network credentials.
3. After STA obtains IPv4, provisioning stops and MyBot starts.
4. An unclaimed device displays and announces its pairing code.
5. Use the configured conversation key to start or stop a multimodal session.

The Robot V2 reference uses two resistor-ladder ADC channels. A key is identified by
its ADC channel and measured voltage window:

| Key | ADC | Window | Function |
| --- | --- | ---: | --- |
| S1 | CEN | — | Hardware reset |
| S2 | ADC15 / GPIO13 | 500-1500 mV | Short: start/stop conversation; long: reprovision |
| S3 | ADC15 / GPIO13 | 4500-6000 mV | Long: factory reset |
| S4 | ADC4 / GPIO28 | 500-1500 mV | Volume down |
| S5 | ADC4 / GPIO28 | 4500-6000 mV | Volume up |

`CONFIG_ADC_KEY_LONG_PRESS_MS` is currently 2000 ms. On a board with different divider
values, calibrate the `MYBOT_KEY_S*_MV_*` settings from the logged channel and millivolt
reading.

## Configuration

Main configuration files:

| File | Purpose |
| --- | --- |
| `projects/mybot/ap/config/bk7259_ap/defconfig` | AP features, language, keys, display, audio, and video |
| `projects/mybot/cp/config/bk7259/defconfig` | CP features |
| `projects/mybot/partitions/bk7259/auto_partitions.csv` | Flash layout |
| `projects/mybot/partitions/bk7259/ram_regions.csv` | SRAM/PSRAM layout |

Important AP options:

- `CONFIG_MYBOT_LANGUAGE_ZH_CN` or `CONFIG_MYBOT_LANGUAGE_EN_US`: selects UI text,
  prompt assets, service endpoint, and the single regional TLS root.
- `CONFIG_MYBOT_VIDEO`: enables camera capture and H.264 uplink.
- `CONFIG_MYBOT_LVGL_UI_ANIMATIONS`: enables face transitions and cursor animation.
- The `MYBOT_KEY_S*` settings select ADC channels, voltage windows, and key functions.

Default endpoints:

```text
https://botstation.shengwang.cn/api   # zh-CN
https://botstation.agora.io/api       # en-US
```

Use the project guide for the JD9855 and Robot V2-specific hardware tables.

## Tests and Validation

Source and host-side checks:

```bash
python3 scripts/check_sources.py
for script in projects/mybot/tests/*/run.sh; do
  bash "$script"
done
(cd scripts && python3 -m unittest test_build_all.py test_vendor_workspace.py)
```

The current host suites cover build preparation, expression policy, HTTPS deadlines
and socket errors, factory-reset persistence, reset ownership, and Wi-Fi/portal faults.
They do not replace target hardware tests.

Before production deployment, validate:

- Cold boot and repeated MyBot start/stop.
- Robot V2 ADC key detection and long-press functions.
- Wi-Fi provisioning, cancellation, reconnect, and power-loss recovery.
- Dual-DMIC capture, hardware AEC, volume, and bidirectional RTC audio.
- Camera capture, H.264 output, frame pacing, bitrate changes, IDR, and repeated video.
- JD9855 orientation, colors, rounded-corner safe area, and both UI languages.
- Concurrent audio, video, UI, HTTP, and RTC load with measured stack, heap, PSRAM, and
  timing headroom.
- The deployed certificate chain, hostname, expiry, and timeout behavior.

Known limitations:

- The provisioning SoftAP is open and serves its local setup page over HTTP.
- Wi-Fi and device credentials are stored in EasyFlash without at-rest encryption.
- The target header defines separate RTC and RTM token concepts; confirm the service
  contract before claiming end-to-end RTM interoperability.
- Video and UI resource peaks still require target measurement.
- Third-party redistribution terms must be resolved before publishing firmware.

See [validation status](projects/mybot/UI_VALIDATION.md) for the current checks and
release blockers.

## Documentation

- [BK7259 MyBot project guide](projects/mybot/README.md)
- [BK dependencies and patches](docs/OFFICIAL_DEPENDENCIES.md)
- [MyBot SDK](https://github.com/junlon2006/mybot)
- [MyBot porting guide](https://github.com/junlon2006/mybot/blob/main/docs/PORTING.md)
- [MyBot embedded integration guide](https://github.com/junlon2006/mybot/blob/main/docs/EMBEDDED.md)
- [BK7259 SDK and solution documentation](bk_solution_ai/docs/bk7259/)

## Contributing

- Send platform-independent SDK and public API changes to the upstream MyBot project.
- Send BK7259 board, adapter, project, and build changes to this repository.
- Keep vendor changes in the recorded patch set.
- Include the hardware, configuration, and validation performed with each change.
- Do not commit generated build products, local references, credentials, or tokens.

## License and Third-Party Dependencies

Original content in this top-level repository is licensed under the
[Apache License 2.0](LICENSE). This does not relicense the submodules, vendored
snapshots, fonts, audio assets, or prebuilt binaries.

Components with separate or additional terms include:

- [BK AVDK license](bk_avdk_smp/LICENSE)
- [BK Solution license](bk_solution_ai/LICENSE)
- [AOSL license and additional terms](components/mybot/mybot_aosl/aosl/LICENSE)
- [LVGL license](bk_avdk_smp/ap/components/lvgl/LICENCE.txt)
- [Display and font provenance](components/mybot/mybot_sdk/platforms/bk7259/display/SOURCES.md)
- [BK7259 third-party notices](components/mybot/mybot_sdk/THIRD_PARTY_NOTICES.md)
- [MyBot third-party notices](https://github.com/junlon2006/mybot/blob/main/THIRD_PARTY_NOTICES.md)

AOSL has conditions in addition to Apache-2.0, so the combined project is not
uniformly Apache-2.0 licensed.

The repository contains the Agora RTSA v1.10.1 build `1278380` input under
`components/mybot/mybot_rtsa/`. The package has no standalone redistribution grant in
this repository. Obtain the required written rights before publishing the binary or
firmware containing it.
