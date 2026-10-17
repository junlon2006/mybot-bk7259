# MyBot 工程（BK7259）

- [English](./README.md)

## 1. 工程简介

`mybot` 是 MyBot SDK 的 BK7259 AP/CP 双核固件集成工程，在同一次会话中提供设备服务控制、
RTC 双向音频、本地配网与界面，以及单向编码视频上行。

内嵌 SDK 同步上游 `main` 最新已提交快照
`39d5ed122aab51d90c1c531298c4ead8dc35b104`（版本1.2.0），保留 HTTPS body 日志补丁。
通过 RTM 进行 MCP 控制时，仅接受
单层 `message.user` 信封，`data_type` 为 `message`，字符串 `content` 内包含
DeviceCommand v1 的 `audio.set_volume`（`params.level` 为 0–100 的整数）或
`audio.adjust_volume`（`params.delta` 为 -100–100 的整数）。正数增加音量，负数减少音量，
0 保持不变，结果钳制到 0–100。发送方、频道、会话、代次校验及最近 16 个不同 command ID
的 FIFO 去重保持生效，重复 ID 不会重复增减音量；裸命令、静音及情绪命令不再通过 RTM
执行，`issued_at`/`expires_at` 仍可省略且忽略。
绑定轮询/停止通知由后台服务队列执行，本地挂断先于HTTP通知完成。像素眼镜 UI 保留
本地表情及声纹/服务端阶段处理，当前快照关闭云端 MCP 情绪入口。AVDK、音频、按键、
视频、电源、GPIO及分区保留旧硬件配置。

## 2. AP 与 CP 职责

| 核 | 职责 |
| --- | --- |
| AP（`bk7259_ap`） | 负责产品控制循环、APSTA 配网、MyBot SDK 与 AOSL 生命周期、HTTPS/RTC/RTM、音频、LCD、按键、摄像头采集、H.264 编码和视频上行。 |
| CP（`bk7259`） | 初始化 BK 运行环境并启动 AP 系统。 |

`bk7259` 工程目标会同时编译并打包两个核。

## 3. 编译

父仓库 gitlink 与 `vendor.lock.json` 固定 BK 官方依赖版本。在 `mybot-bk7259` 仓库根目录执行：

```bash
python3 scripts/build_all.py
```

脚本准备官方源码隔离副本，应用已记录的 DNS 和摄像头辅助补丁，再分别编译两种语言的
AP/CP；不修改官方子模块 tracked 源码。使用 `--language zh-CN` 或 `--language en-US`
可只编译一种语言；默认 clean，`--no-clean` 为增量构建。

手工调用 `make` 时，先准备工作区，再执行工具打印的完整命令，包含其生成的 `SDK_DIR`：

```bash
python3 scripts/prepare_vendor.py --language zh-CN
```

发布的完整烧录镜像为：

```text
releases/bk7259-zh-CN-video.bin
releases/bk7259-en-US-video.bin
```

中间产物 `all-app.bin`、`app_pack.rbl` 与 `build_summary.txt` 位于
`build/build-bk7259-<language>-video/solution/projects/mybot/build/bk7259/mybot/package/`。
各核 ELF 位于对应生成工程下的 `build/bk7259/mybot/bk7259_ap/` 和
`build/bk7259/mybot/bk7259/`。产品源码由父仓库维护，应在源目录修改，而不是修改生成副本。
详见[官方依赖维护说明](../../docs/OFFICIAL_DEPENDENCIES.md)。

## 4. 多模态视频上行

AP 默认配置已开启 `CONFIG_MYBOT_VIDEO=y`。BK7259 视频链路为：

```text
MIPI CSI GC2053/CV2005 传感器（1280x720，5 fps）
  -> ISP MP（640x480 NV12）
  -> 硬件 FLEXA H.264 编码器
  -> 编码帧池
  -> MyBot 视频处理器
  -> Agora RTSA 主视频流
```

当前参数与行为：

| 项目 | 当前值 |
| --- | --- |
| 编码输出 | H.264 Annex-B access unit，640x480 |
| 传感器输入 | 1280x720，5 fps |
| 码率范围 | 256000-512000 bit/s，初始中点为 384000 bit/s |
| GOP | 30 帧；5 fps 下约 6 秒，RTSA 关键帧请求可强制生成 IDR |
| 生命周期 | RTC 到达 `CONNECTED` 后才启动采集和编码；离开 RTC 前先停止 |
| 方向 | 仅设备视频上行，远端视频订阅已关闭 |
| RTSA 时序 | `frame_rate=0`，由 RTSA 使用真实帧时间戳，不按固定帧率合成时间戳 |

RTSA 的带宽估算结果会被限制在配置的码率范围内，再应用到硬件编码器。音频保持双向，
视频仅上行。

视频配置位于：

- `components/mybot/mybot_sdk/Kconfig`
- `projects/mybot/ap/config/bk7259_ap/defconfig`

修改视频配置后应执行完整重新编译，避免生成配置保留旧值。

## 5. 显示与表情

JD9855保留320x385物理分辨率和385x320逻辑RGB565画面，GPIO53供电、GPIO5复位、GPIO7
背光低有效。LVGL视图使用模拟器工艺灰皮肤与像素眼镜，四行终端文字显示表情、运行状态及
会话阶段/声纹状态；启动、配网、网页控制台配对及错误保留中英文引导，恢复出厂增加恢复中、
失败重试和重启提示，优先于 SDK 更新。S3 保留原 ADC 长按配置；恢复意图支持断电续做，只有
SDK 与网络停止后才逐项删除并校验三条 MyBot 记录。网络或 SDK 启动失败仍可用 S2 重新配网、
S3 恢复出厂，并保留 LCD 提示。
图案等比缩放为346x302，位于(19,9)；终端文字9px、签名10px，四行保持独立行高。
配对码固定为6位数字，使用固定32px字号。
屏幕四角有明显圆弧，终端文字、配对码与眼镜主体须位于可见安全区；照片未提供精确硬件半径。
旧浅色主题选项仅保留配置兼容，新像素渲染器固定使用工艺灰外观。

| 运行状态 | 表情与时间 |
| --- | --- |
| READY / 挂断 | NEUTRAL |
| 进入会话 | SURPRISED保持300 ms，另计进入过渡 |
| LISTENING | LISTENING |
| THINKING | THINKING；连续三秒后PROCESSING |
| SPEAKING / 无阶段 | NEUTRAL |

开启动画时，会话开场保持时间另加250 ms进入过渡。普通状态和声纹更新不重置开场保持
计时；当前 SDK 快照关闭云端 MCP 情绪入口。开场到期恢复
最新自动阶段，后台THINKING已持续三秒时恢复PROCESSING。离开会话清除计时，必要引导与
恢复出厂优先。表情只控制LCD图案，不发送电机命令。

全部23个表情名称如下：

```text
NEUTRAL, GREETING, HAPPY, EXCITED, CURIOUS, CONFUSED, THINKING, LISTENING,
SURPRISED, SHY, PROUD, LOVE, SLEEPY, BORED, SAD, PLEADING, APOLOGY, WORRIED,
ANNOYED, REFUSE, PROCESSING, ERROR, SLEEP
```

`include/bk7259_expression.h`提供名称转换及复制投递的本地接口
`bk7259_lcd_set_expression()` / `bk7259_lcd_clear_expression()`。本地覆盖保持至清除或流程
切换，重复同一本地覆盖不重播；清除后恢复最新自动表情。ERROR、SLEEP图案
不改变实际运行状态。渲染、动画和到期处理均由现有显示owner执行，不新增UI线程或UTC依赖。

## 6. 真机验收

- [ ] 冷启动组合镜像 `all-app.bin`，确认 AP 和 CP 均正常启动。
- [ ] 确认板上 GC2053 或 CV2005 摄像头识别成功，视频源报告 1280x720、5 fps，并输出 640x480 H.264。
- [ ] 开启会话，确认双向音频正常，远端能收到 H.264 视频。
- [ ] 确认摄像头采集和编码只在 RTC 连接后启动；会话拆除开始后不再发送视频帧。
- [ ] 实测帧率约 5 fps、编码码率位于 256-512 kbit/s，初始目标为 384 kbit/s。
- [ ] 确认周期 IDR 间隔约 6 秒，并验证 RTSA 关键帧请求会强制生成新的 IDR。
- [ ] 在受限 Wi-Fi 下确认 RTSA 码率调整始终位于 256-512 kbit/s，且不会破坏音频通路。
- [ ] 反复结束、重启和中断会话，确认编码器、帧池、摄像头及 RTC 资源均能正常停止并再次启动。
- [ ] 在385x320逻辑屏验证全部23表情、四行文字、中英文及配对引导，确认终端文字、配对码与眼镜主体不被四个实体圆角遮挡。
- [ ] 验证SURPRISED开场、阶段切换及THINKING连续三秒超时；挂断应恢复NEUTRAL。
- [ ] 通过 `message.user` 信封验证 MCP `audio.set_volume` 的 0、50、100 音量；裸命令、静音及情绪命令应无效果。
- [ ] 验证封装后的 `audio.adjust_volume` 正数、负数及 0，包含 -100、100 边界；结果应钳制到 0–100，重复 command ID 不应再次增减音量。
- [ ] 验证本地表情设置/清除及重复覆盖，且无电机输出；重复阶段/声纹更新不重置会话开场计时。
- [ ] 动画运行时回归原四个ADC按键、长按配网/恢复出厂及音视频功能。
