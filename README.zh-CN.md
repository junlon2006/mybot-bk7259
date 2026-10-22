# mybot-bk7259

[![License](https://img.shields.io/github/license/junlon2006/mybot-bk7259)](LICENSE)
[![Upstream](https://img.shields.io/badge/upstream-mybot-0969da)](https://github.com/junlon2006/mybot)
[![Platform](https://img.shields.io/badge/platform-BK7259-2f6f4e)](https://github.com/bekencorp/bk_avdk_smp)

**简体中文 | [English](README.md)**

`mybot-bk7259` 是 [MyBot SDK](https://github.com/junlon2006/mybot) 面向 Beken BK7259
AP/CP 平台的参考固件集成。`main` 分支面向 Robot V2 参考板，提供板级初始化、网络所有权、
外设适配、固件构建文件和本地产品策略；设备服务注册、鉴权、配对、会话状态、RTC/RTM 和
声纹处理由 MyBot SDK 负责。

本项目是参考实现，不是可直接量产的产品固件。部署前仍需完成板级适配、安全审查、
目标硬件测试和第三方许可证审查。

## 功能

- BK7259 AP/CP 启动、Flash 分区以及 SRAM/PSRAM 布局。
- 产品层负责的 STA 重连和带强制门户的 APSTA Wi-Fi 配网。
- 双 DMIC 采集、片上硬件 AEC 和扬声器播放。
- 可选 MIPI CSI 摄像头采集、硬件 H.264 编码和 Agora RTC 视频上行。
- 电阻梯 ADC 按键、JD9855 MIPI 显示、EasyFlash 存储、HTTPS 和派生的设备身份。
- 内嵌中英文提示音资源。
- 统一构建入口，可生成中英文 AP/CP 镜像、组合烧录镜像和 OTA 包。

设备服务和云端 AI Agent 是外部依赖，不在本仓库范围内。

## 架构

| 组件 | 职责 |
| --- | --- |
| CP（`bk7259`） | 初始化 BK 运行环境并启动 AP 系统。 |
| AP（`bk7259_ap`） | 运行产品控制循环、网络、MyBot 生命周期、音频、显示、按键、摄像头和 H.264 编码。 |
| MyBot SDK | 负责设备服务注册、鉴权、配对、会话状态、RTC/RTM 和声纹处理。 |
| BK7259 平台适配 | 使用 BK 组件和板级资源实现 MyBot 平台 `ops`。 |
| AOSL | 为 MyBot 和 RTSA 提供可移植运行时。 |

产品控制循环位于 [ap_main.c](projects/mybot/ap/ap_main.c)。它从芯片 UID 派生设备 ID，
准备产品硬件，恢复或执行 Wi-Fi 配网，在 STA 获得 IPv4 后启动 MyBot，并负责恢复出厂和
产品级恢复。

BK 平台源码只通过 `<mybot/platform/...>` 公共平台头文件访问 MyBot，公开的 MyBot
生命周期接口由 AP 应用入口统一管理。

## 仓库结构

| 路径 | 内容 |
| --- | --- |
| `bk_avdk_smp/` | 固定版本的 BK7259 AP/CP SDK 子模块 |
| `bk_solution_ai/` | 固定版本的 BK Solution 子模块 |
| `components/mybot/mybot_sdk/` | MyBot SDK 快照和 BK7259 平台适配 |
| `components/mybot/mybot_aosl/` | 内嵌 AOSL 运行时 |
| `components/mybot/mybot_rtsa/` | Agora RTSA 头文件、包信息和目标库 |
| `projects/mybot/` | AP/CP 入口、板级配置、分区、资源和测试 |
| `patches/` | 仅应用于生成构建副本的审核后厂商补丁 |
| `scripts/` | 依赖准备、源码校验和构建工具 |
| `docs/` | 依赖和验证文档 |

`build/`、`releases/` 等生成目录不会进入版本控制。本地参考资料不是构建输入。

## 固定依赖

| 依赖 | 版本 |
| --- | --- |
| `bk_avdk_smp` | `584b947920c414230f9909375f008e7d6a2c6c6b` |
| `bk_solution_ai` | `03aadb49756a5109bfede04002b9b6820f407c3f` |
| MyBot SDK 快照 | `58f0ef9c14be9f159b1dc167bf0f035983b25117`，版本 1.3.0 |
| AOSL 基线 | `84e086084ebcd0ae2455a0ce5721950c5fe2e656` |
| Agora RTSA | v1.10.1 build `1278380` |

父仓库通过 `vendor.lock.json` 固定子模块 gitlink，通过 `SDK_REVISION` 和
`AOSL_REVISION` 记录源码修改，并通过
`components/mybot/mybot_rtsa/PACKAGE_INFO` 记录 RTSA 包。构建时在隔离副本中应用厂商补丁，
不修改官方子模块 checkout。

源码角色、补丁来源和更新规则见 [BK 依赖与厂商补丁](docs/OFFICIAL_DEPENDENCIES.zh-CN.md)。

## 环境要求

- 支持 Git submodule 的 Linux 环境。
- Python 3.9 及以上版本，以及固定 `bk_avdk_smp` 所需的构建依赖，包括
  `click_option_group`。
- `bk_avdk_smp` 选择的 BK7259 Arm GNU 工具链。
- 按 Robot V2 接线的 BK7259 开发板，或等价硬件。
- 兼容的 MyBot 设备服务和已启用 Agora 服务的部署环境。

## 获取源码

```bash
git clone --recurse-submodules https://github.com/junlon2006/mybot-bk7259.git
cd mybot-bk7259
```

已有仓库可通过以下命令初始化子模块：

```bash
git submodule sync --recursive
git submodule update --init --recursive
git status
git submodule status
```

正常开发使用父仓库固定的 gitlink，不要用 `git submodule update --remote` 跟踪移动分支。

## 构建

在仓库根目录执行：

```bash
python3 scripts/check_sources.py
python3 scripts/build_all.py
```

`check_sources.py` 校验子模块版本和已跟踪文件状态、补丁摘要、MyBot/AOSL 内容摘要、
RTSA 文件摘要以及产品源码边界。`build_all.py` 准备隔离厂商工作区，为每种语言编译两个核，
校验生成配置和服务地址，并在全部请求的变体成功后发布产物。

常用选项：

```bash
python3 scripts/build_all.py --language zh-CN
python3 scripts/build_all.py --language en-US
python3 scripts/build_all.py --dry-run
python3 scripts/build_all.py --no-clean
```

固定版本的 RTSA 包只支持 `MYBOT_AUDIO_PTIME_MS=60`。

### 构建产物

默认输出为：

```text
releases/bk7259-zh-CN-video.bin
releases/bk7259-en-US-video.bin
```

中文构建的生成产物位于：

```text
build/build-bk7259-zh-CN-video/solution/projects/mybot/build/bk7259/mybot/package/
```

| 文件 | 用途 |
| --- | --- |
| `all-app.bin` | 包含 bootloader、CP 和 AP 的组合烧录镜像 |
| `app_pack.rbl` | 包含 CP 和 AP 镜像的 OTA 包 |
| `build_summary.txt` | Flash 和 RAM 使用报告 |

独立 CP 和 AP 镜像位于同一生成构建目录下的 `bk7259/app.bin` 和
`bk7259_ap/app.bin`。

直接调用厂商构建时，先生成对应语言的工程：

```bash
python3 scripts/prepare_vendor.py --language zh-CN
```

随后执行工具打印的完整 `make` 命令。不要在未经补丁准备的厂商目录中直接构建源工程。

## 烧录与使用

使用 Beken 烧录工具写入 `all-app.bin`；具体步骤见 `bk_avdk_smp` 随附的烧录文档。

典型流程：

1. 没有有效 Wi-Fi 凭据时，设备启动开放热点 `mybot-xxxx`。
2. 连接热点并打开 `http://192.168.4.1/`，提交目标网络凭据。
3. STA 获得 IPv4 后，配网门户停止，MyBot 启动。
4. 未绑定设备显示并播报配对码。
5. 使用配置的对话键开始或结束一次多模态会话。

Robot V2 使用两条 ADC 电阻梯。按键由 ADC 通道和电压窗口共同识别：

| 按键 | ADC | 电压窗口 | 功能 |
| --- | --- | ---: | --- |
| S1 | CEN | — | 硬件复位 |
| S2 | ADC15 / GPIO13 | 500-1500 mV | 短按开始/结束会话；长按重新配网 |
| S3 | ADC15 / GPIO13 | 4500-6000 mV | 长按恢复出厂 |
| S4 | ADC4 / GPIO28 | 500-1500 mV | 音量减 |
| S5 | ADC4 / GPIO28 | 4500-6000 mV | 音量加 |

`CONFIG_ADC_KEY_LONG_PRESS_MS` 当前为 2000 ms。分压值不同的板型应根据日志中的通道和
毫伏值校准 `MYBOT_KEY_S*_MV_*`。

## 配置

主要配置文件：

| 文件 | 用途 |
| --- | --- |
| `projects/mybot/ap/config/bk7259_ap/defconfig` | AP 功能、语言、按键、显示、音频和视频 |
| `projects/mybot/cp/config/bk7259/defconfig` | CP 功能 |
| `projects/mybot/partitions/bk7259/auto_partitions.csv` | Flash 布局 |
| `projects/mybot/partitions/bk7259/ram_regions.csv` | SRAM/PSRAM 布局 |

AP 主要选项：

- `CONFIG_MYBOT_LANGUAGE_ZH_CN` 或 `CONFIG_MYBOT_LANGUAGE_EN_US`：选择界面文本、提示音、
  服务地址和单地区 TLS 根证书。
- `CONFIG_MYBOT_VIDEO`：启用摄像头采集和 H.264 视频上行。
- `CONFIG_MYBOT_LVGL_UI_ANIMATIONS`：启用表情过渡和光标动画。
- `MYBOT_KEY_S*` 设置按键 ADC 通道、电压窗口和功能。

默认服务地址：

```text
https://botstation.shengwang.cn/api   # zh-CN
https://botstation.agora.io/api       # en-US
```

JD9855 和 Robot V2 的详细硬件配置见工程说明。

## 测试与验证

源码和主机侧检查：

```bash
python3 scripts/check_sources.py
for script in projects/mybot/tests/*/run.sh; do
  bash "$script"
done
(cd scripts && python3 -m unittest test_build_all.py test_vendor_workspace.py)
```

当前主机测试覆盖构建准备、表情策略、HTTPS 超时和 socket 错误、恢复出厂持久化、
恢复所有权以及 Wi-Fi/门户故障。主机测试不能替代目标硬件验证。

量产前至少完成：

- 冷启动和反复启动、停止 MyBot。
- Robot V2 ADC 按键检测及长按功能。
- Wi-Fi 配网、取消、重连和掉电恢复。
- 双 DMIC 采集、硬件 AEC、音量和 RTC 双向音频。
- 摄像头采集、H.264 输出、帧率、码率调整、IDR 和重复视频会话。
- JD9855 方向、颜色、圆角安全区和中英文界面。
- 音频、视频、界面、HTTP 和 RTC 并发时测量栈、堆、PSRAM 和时序余量。
- 实际服务的证书链、主机名、有效期和超时。

当前限制：

- 配网 SoftAP 为开放热点，本地设置页面使用 HTTP。
- Wi-Fi 和设备凭据保存在 EasyFlash 中，没有静态加密。
- 目标头文件区分 RTC 和 RTM token，在声明端到端 RTM 互通前需确认服务契约。
- 视频和 UI 运行期资源峰值仍需目标测量。
- 发布固件前必须解决第三方组件的再分发条款。

当前检查和发布阻塞项见 [验证状态](projects/mybot/UI_VALIDATION.zh-CN.md)。

## 文档

- [BK7259 MyBot 工程说明](projects/mybot/README_CN.md)
- [BK 依赖与厂商补丁](docs/OFFICIAL_DEPENDENCIES.zh-CN.md)
- [MyBot SDK](https://github.com/junlon2006/mybot)
- [MyBot 移植指南](https://github.com/junlon2006/mybot/blob/main/docs/PORTING.zh-CN.md)
- [MyBot 嵌入式集成指南](https://github.com/junlon2006/mybot/blob/main/docs/EMBEDDED.zh-CN.md)
- [BK7259 SDK 与解决方案文档](bk_solution_ai/docs/bk7259/)

## 贡献

- 平台无关的 SDK 功能和公开 API 修改提交到上游 MyBot 项目。
- BK7259 板级、适配层、工程和构建修改提交到本仓库。
- 厂商修改必须保留在受版本锁约束的补丁集中。
- 每次提交需说明所用硬件、配置和实际完成的验证。
- 不要提交构建产物、本地参考资料、凭据或 token。

## 许可证与第三方依赖

本顶层仓库的原创内容采用 [Apache License 2.0](LICENSE)。该许可证不覆盖子模块、内嵌源码
快照、字体、音频资源或预编译二进制。

具有独立或附加条款的组件包括：

- [BK AVDK 许可证](bk_avdk_smp/LICENSE)
- [BK Solution 许可证](bk_solution_ai/LICENSE)
- [AOSL 许可证及附加条款](components/mybot/mybot_aosl/aosl/LICENSE)
- [LVGL 许可证](bk_avdk_smp/ap/components/lvgl/LICENCE.txt)
- [显示和字体来源](components/mybot/mybot_sdk/platforms/bk7259/display/SOURCES.md)
- [BK7259 第三方声明](components/mybot/mybot_sdk/THIRD_PARTY_NOTICES.md)
- [MyBot 第三方声明](https://github.com/junlon2006/mybot/blob/main/THIRD_PARTY_NOTICES.md)

AOSL 在 Apache-2.0 之外还有附加条件，因此组合工程不能整体描述为仅采用 Apache-2.0。

仓库在 `components/mybot/mybot_rtsa/` 中包含 Agora RTSA v1.10.1 build `1278380`
输入。当前仓库中没有该包的独立再分发授权；发布二进制或包含它的固件前，必须取得所需的
书面授权。
