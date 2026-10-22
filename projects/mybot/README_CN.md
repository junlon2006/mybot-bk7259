# MyBot 工程（BK7259）

- [English](./README.md)
- [仓库概览](../../README.zh-CN.md)
- [验证状态](./UI_VALIDATION.zh-CN.md)

## 范围

本目录保存 MyBot SDK 在 Robot V2 参考板上的 BK7259 产品集成，负责板级初始化、
APSTA/STA 网络、本地外设、产品策略和 MyBot 生命周期边界。

设备服务注册、鉴权、配对、会话状态、RTC/RTM 和声纹处理由 MyBot SDK 负责。
云端服务和 AI Agent 属于外部依赖。

## AP 与 CP

| 核 | 职责 |
| --- | --- |
| CP（`bk7259`） | 初始化 BK 运行环境并启动 AP 系统。 |
| AP（`bk7259_ap`） | 运行产品任务、网络、MyBot 生命周期、音频、显示、按键、摄像头和 H.264 编码。 |

`bk7259` 工程目标同时编译并打包两个核。

## 启动与网络所有权

AP 入口为 [ap_main.c](./ap/ap_main.c)：

1. 初始化 BK 运行环境和媒体服务。
2. 创建 `mybot_app` 产品任务。
3. 从芯片 UID 派生设备 ID，构建运行时配置。
4. 初始化 AOSL，并准备产品自有的显示和按键管理。
5. 恢复待执行的恢复出厂操作、连接已保存 Wi-Fi，或进入 APSTA 配网。
6. STA 获得 IPv4 后启动 MyBot。
7. 轮询 SDK 状态并处理会话、配网和恢复出厂请求。

Wi-Fi 由产品层拥有。SDK 适配层只转发串行化后的连接状态。重新配网必须等待
`mybot_stop()`，并确认 `STOPPED` 且 `mybot_is_running() == false` 后才能启动 APSTA。

网络或 SDK 启动失败时，产品保留显示和按键，允许 S2 重新配网或 S3 恢复出厂。

## ADC 按键与恢复出厂

Robot V2 使用两条 ADC 电阻梯，按键由 ADC 通道和电压窗口共同识别：

| 按键 | ADC | 电压窗口 | 短按 | 长按 |
| --- | --- | ---: | --- | --- |
| S1 | CEN | — | 硬件复位 | — |
| S2 | 15 / GPIO13 | 500-1500 mV | 开始或结束会话 | 重新配网 |
| S3 | 15 / GPIO13 | 4500-6000 mV | — | 恢复出厂 |
| S4 | 4 / GPIO28 | 500-1500 mV | 音量减 | — |
| S5 | 4 / GPIO28 | 4500-6000 mV | 音量加 | — |

`CONFIG_ADC_KEY_LONG_PRESS_MS` 当前为 2000 ms。ADC 引脚保持高阻且不使用内部上拉，
避免加载分压器。GPIO8 和 GPIO9 是 32.768 kHz 晶振引脚，保持高阻。

恢复出厂由产品任务执行：

1. 停止 MyBot 并确认进入 `STOPPED`。
2. 停止配网门户、扫描和 STA。
3. 删除并校验 `mybot.device_auth`、`mybot.wifi.v1` 和 `mybot.volume.v1`。
4. 删除恢复标记并重启。

恢复出厂不会清除无关 EasyFlash 记录，也不改变 UID 派生的设备 ID。中途断电时，下次启动
会在网络或 SDK 启动前继续清理。清理失败时保留意图，等待用户再次长按 S3。

## 音频

MyBot 使用 16 kHz、单声道、有符号 16 位 PCM。Robot V2 采集路径使用双 DMIC 输入和
片上硬件 AEC，播放使用板载 DAC。

音量变化通过 SDK 设备音量适配层处理并保存到 EasyFlash。S5/S4 分别提供音量加/减。
采集、播放、AEC 和反复会话循环仍需目标板验证。

## 显示

产品层从平台准备到关闭期间独占 JD9855 MIPI 屏，因此 MyBot 停止时配网页面仍然可见。
原生 320x385 面板以 385x320 逻辑 RGB565 画面渲染。

板级资源：

- GPIO53：屏幕供电
- GPIO5：屏幕复位
- GPIO7：低有效背光

LVGL 9.5.0 界面采用工艺灰和像素眼镜风格，包含四行终端文字，显示配网、配对、就绪、
会话阶段、声纹、错误和恢复出厂状态，并支持中英文提示。

界面为屏幕圆角保留保守安全区，实际硬件圆角半径尚未精确测量。GPU 和触摸关闭。
资源来源见
[display/SOURCES.md](../../components/mybot/mybot_sdk/platforms/bk7259/display/SOURCES.md)。

## 视频

默认 AP 配置启用视频，链路为：

```text
GC2053/CV2005 MIPI CSI，1280x720，5 fps
  -> ISP MP 缩放为 640x480 NV12
  -> 硬件 FLEXA H.264 编码
  -> 完整 Annex-B access unit
  -> MyBot 视频处理器
  -> Agora RTC 上行
```

码率范围为 256000-512000 bit/s，初始目标 384000 bit/s。视频仅在 RTC `CONNECTED`
后启动，并在离开 RTC 前停止。设备不接收或显示远端视频。

配置和实现见 [defconfig](./ap/config/bk7259_ap/defconfig)与
[bk7259_video.c](../../components/mybot/mybot_sdk/platforms/bk7259/bk7259_video.c)。

## 验证

主机测试位于 [`tests/`](./tests/)。当前状态和剩余目标板检查见
[UI_VALIDATION.zh-CN.md](./UI_VALIDATION.zh-CN.md)。

测试覆盖表情策略、HTTPS 传输、恢复出厂持久化、恢复所有权和 Wi-Fi/门户故障。
主机测试不能验证 ADC 电压、DMIC/AEC 质量、显示时序、摄像头、掉电或长时间音视频运行。
