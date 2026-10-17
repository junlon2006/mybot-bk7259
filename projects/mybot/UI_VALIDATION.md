# BK7259 LVGL UI 验证记录

`main` 于2026-10-17迁移为官方BK依赖和父仓库产品源码。当前构建入口是仓库根目录的
`python3 scripts/build_all.py`；需要手工make时先运行`python3 scripts/prepare_vendor.py --language zh-CN`，
再执行输出的命令。以下历史记录中的Solution产品路径、fork版本和直接make命令均为当时布局，
不作为当前操作说明。现目录及版本管理见[官方依赖说明](../../docs/OFFICIAL_DEPENDENCIES.md)。

2026-10-17官方依赖迁移后，新的隔离构建已完成中英文AP/CP编译和合包，表情策略测试13组、
462条断言通过；SDK/AOSL/RTSA的185个不可变文件摘要与迁移前一致。本次不修改UI运行逻辑，
尚未重新烧录做实机回归；迁移构建产物位于`releases/official-main/`。详细记录见上方依赖说明。

本文前半保留旧状态卡片UI的历史测试；该视图已由本文末尾2026-10-16记录的旧硬件像素眼镜UI
替换。历史截图、内存与主题测试结果不代表新视图已通过验证，当前实现以最新条目为准。

本次在原工程接入 `mybot-esp32` 的语义状态视图，默认中文、深色、启用状态动画。
实现位于 `components/mybot/mybot_sdk/platforms/bk7259/{bk7259_lcd.c,display/}`；
项目配置为 `ap/lv_conf_custom.h` 和 `ap/config/bk7259_ap/defconfig`。
SSID 由配网模块复制投递。SDK 的 `include/src` 快照已单独同步到锁定版本；AOSL 和 AVDK 源码
未因本次 UI 移植修改。
旧直接渲染字体及其独立许可证已删除，可从 Git 历史恢复；新资源许可见
`components/mybot/mybot_sdk/platforms/bk7259/display/SOURCES.md`。

## UI 移植时的基线与契约

本节保留当时的验证版本。后续 SDK 更新以 `components/mybot/mybot_sdk/SDK_REVISION` 为准。

- AVDK：`239c151bf4ba239bf8ebc1a178df0a8b659b55bd`，工作树保持干净，使用其中的 LVGL 9.5.0。
- Solution：`9cbfb976acdf043b01dde91daf1889a54e66d122`，改动留在工作树，未提交或更新根 gitlink。
- ESP32 视图来源：`19af3bdfaf1dd35461af48def8f9558e3503e755`；未修改参考工程。
- MyBot SDK：`4ae239c804257f8b5c557e5879b54d9a88d80847`，RTC 日志级别和 AOSL 恢复已包含在
  上游快照，HTTPS body 日志仍由 `SDK_REVISION` 记录。`include/src` SHA256 复核为
  `84a5a0a015de7be6a91658ebc3de1f6958a8e595750d859426dd3a7dae1aa76c`。
- AOSL 基线：`84e086084ebcd0ae2455a0ce5721950c5fe2e656` 加 `AOSL_REVISION` 已记录的 BK7259 补丁。
  内容 SHA256 复核为 `2e9fc3fa5571972fd31c1ab64546477a98550b6b210e38fec181c0f4daeae64e`。
- 既有 RTSA 包：BK7259 v1.10.1 build 1278380，未更换；完整构建保留其头文件和库的哈希校验。
- 已核对锁定 SDK 的完整 `docs/PORTING.zh-CN.md`、`docs/EMBEDDED.zh-CN.md` 和
  `include/mybot/platform/mybot_lcd.h`，以及 BK 的显示、DPU、帧分配和 RTOS 接口。
  SDK 输入只按平台头文件消费；SDK 销毁只解挂，产品显示持续到平台关闭。

## 2026-09-23 SDK 同步复核

- 通过远端 `main` 确认最新提交为 `83fbcb0969da4c73a5912326d699f90ec634b28e`，版本号仍为 1.2.0。
  全量同步 56 个 `include/src` 文件；唯一目标补丁仍为原有 HTTPS body 日志，补丁内容不变。
  新聚合 SHA256：`3f89476f880518b0f0d68e45d709023a0cb0d6146865d8e32b8a95ceb4e46b24`。
- 此次修改基于 Solution `d717cef1879695cafdc83c436c7abeda692e4196`，未自动提交。
  AVDK、AOSL 和 RTSA 仍使用本记录中的固定版本；AOSL 内容与 RTSA 头文件/静态库摘要复核一致。
  BK7259 平台、圆角 UI 和 UID 设备 ID 源码未改变，无新增 GCC 原子接口依赖。
- 核对最新 `PORTING.zh-CN.md`、`EMBEDDED.zh-CN.md` 和应用生命周期、音频、视频、Wi-Fi、
  LCD、提示音公开头文件。视频控制改由 SDK 控制线程串行执行，提示音资源保留到 RTC 回调
  排空后销毁；现有平台 ops 和目标 CMake 源文件列表无需调整。
- 在隔离宿主目录以 `MYBOT_ENABLE_VIDEO=ON` 构建并运行现有 `ctest`，21/21 通过。
  测试提交 `655bf9c5aa53ea90561b0ad61af0ddd97a1d9c32` 与最终远端合并提交的完整 Git tree
  均为 `7173ad69ca9348319a8131a57053bd9bdb2381f9`。覆盖视频启停/回调、提示音 stop/destroy、
  RTM 三状态/声纹以及设备生命周期和 JSON；这些宿主测试使用上游源，不包含目标 HTTPS 日志补丁。
- 原工程执行下述 clean/build 命令，带目标补丁的 AP/CP 完整编译与合包通过。
  AP Flash 1,926,024 字节（41.00%），AP 静态 RAM 152,900 字节（38.88%）；
  CP Flash 1,296,900 字节（89.95%）。ELF 为 ARM32 小端 hard-float，无未解析符号。
  `all-app.bin` 为 3,433,376 字节，`app_pack.rbl` 为 2,027,888 字节。
- 逐文件验证确认除声明补丁外与上游完全一致，`git diff --check` 通过。
  旧边界检查器仍因固定旧版本和“不使用 LVGL”的旧规则失败，未将其记作通过。
  Kconfig 和链接器既有警告仍存在。此次未重新执行 UI 宿主截图测试或实板音视频验证。

## 构建入口与历史结果

当前在根目录执行以下入口；默认清理生成工程后重建：

```sh
env CCACHE_DISABLE=1 python3 scripts/build_all.py
```

以下是早期状态卡片UI的历史构建结果，不是2026-10-17迁移后的新测量：AP、CP 完整构建和
合包通过，最终增量构建也通过。`all-app.bin` 用于完整烧录，`app_pack.rbl` 为 OTA 包。
当时ELF为ARM、32位、小端、hard-float ABI。当前产物应在
`build/build-bk7259-<language>-video/solution/projects/mybot/build/bk7259/mybot/package/` 查找。

| 区域 | 使用量 | 分区占用 |
| --- | ---: | ---: |
| AP Flash | 1,926,128 字节 | 41.00% |
| AP 静态 RAM | 152,908 字节 | 38.89% |
| CP Flash | 1,296,900 字节 | 89.95% |

相对移植前记录，AP Flash 增加 324,144 字节，静态 RAM 增加 792 字节。
运行时另需双扫描缓冲 492,800 字节、绘制条带 12,320 字节和 UI 栈 8,192 字节。
最终链接未引入 vendor LVGL 显示线程、FreeRTOS 绘图线程或 GPU 绘制后端。
工具链仍报告既有 `_gettimeofday` stub、GNU-stack 和 RWX 段警告，不能视为零警告构建。

## 宿主验证

- 真实 LVGL 9 渲染：全部 10 个页面、中英文、深浅主题、三种会话状态、长配对码与
  SSID 滚动、借用输入立即改写、100 次创建/销毁；已检查截图布局。
- 直接包含实际 `bk7259_lcd.c`，替换 HAL/RTOS/LVGL 调用的后端测试：11 组通过。
  覆盖分条/局部旋转和未修改像素保留、双帧占用超时、补绘、提交失败的不同释放语义、
  退出与 deinit 失败后的资源保留和重试、回调退出屏障、快照复制与解挂、首帧失败、
  重新 prepare，以及分配溢出与 OOM。
- 两组测试启用 ASan/UBSan；后端还启用 `-Wall -Wextra -Werror`。
  LSan 因沙箱 ptrace 限制未启用，不作其检测通过的结论。
- 64 位宿主的 LVGL 分配跟踪：创建后 7,286 字节，首次渲染后 7,054 字节，
  真实 UI 工作负载峰值 9,499 字节；三种动画各运行 60 秒，未观察到保留分配增长。
  100 次创建/销毁后基线稳定，最终 `lv_deinit` 后跟踪分配归零。
  这些数值不含帧缓冲、条带、线程栈和分配器开销；宿主使用 24 行条带，目标为 16 行，
  且指针宽度不同，不能替代目标机内存峰值测量。
- LVGL 内部仍会为每次绘制分配/释放任务，动画测得约 240–370 次/秒，单次最大请求
  304 字节；自有动态标签使用固定缓冲。64 KiB 配置只限制绘制层像素，不是 UI 总堆上限。

本机宿主测试及日志保留在 `/tmp/mybot-lvgl-view-test.f9A5v9/` 和
`/tmp/mybot-lcd-regression.M0abjc/`；这些是临时验证文件，不属于固件构建输入。

## 适配边界与上板检查

显示原子变量全部走 BK7259 AOSL HAL。两帧仅由释放回调归还；释放无法确认时保留资源。
若驱动在接收帧前失败又不回调，保守保留可能要求重启，不能擅自假定该帧已释放。
UI 工作线程退出、显示关闭和回调等待均有超时；通知信号量按进程生命周期复用。

锁定 LVGL 的默认绘制分配在 HSRAM 耗尽时回退 PSRAM，但配对释放仍为 HSRAM。
适配层在初始化后只替换默认、字体、图片三组 handler 的分配函数，禁止该回退。
为保留原有像素格式复制、对齐和 stride 回调，使用固定 LVGL 9.5 的
`src/draw/lv_draw_buf_private.h` 访问 handler 字段；升级 LVGL 时需复核此处。

`git diff --check` 通过；SDK/AOSL 摘要和 vendor 干净状态单独复核通过。
旧 `skills/mybot-development/scripts/check_port_boundary.sh` 已运行但未通过：
它仍锁定旧版本/摘要和“不使用 LVGL”的旧方案，亦拒绝既有集成依赖。
本次未修改该本地检查器，也不将它的结果记作通过。

尚未连接实板验证。烧录后重点检查：屏幕方向和 RGB565 颜色、中英文切换、配网/配对、
listening/thinking/speaking 与声纹通知、反复停止/启动 SDK，以及同时开启音视频时的
动画流畅度、HSRAM 余量、线程栈高水位和长时间运行情况。

## 2026-10-16 旧硬件分支SDK同步

在父仓库main基线f30feb279567bf74a1a5abcb15f51c257c467377、Solution
release/v4.0.1-mybot基线48a1c39a2fec974b77b4036ecc1bf93286265b67上，先独立同步SDK，
UI调整留待该阶段提交之后。AVDK固定f699c50f2ee72e7ca014c8f009e0d40f1eeb059a。

SDK完整include/src从064397882187a242be25a525a91df83d353b78bb更新为远端main的
e7793eba88f721304dc00d421706d4d788b81a43（版本1.2.0），共58个文件，保留原有已声明
HTTPS请求/响应body日志补丁。除src/support/mybot_http_client.c外逐文件与所选上游一致；
聚合SHA256为fb7bd88a4d191e3b341c54d8a993b70b4a7b7ea3bd4632411f7899156015fa67。
构建注册新增src/core/mybot_device_command.c，继续由SDK实现命令解析和执行。

DeviceCommand v1支持音量设置/增减、播放静音及HAPPY/SAD/ANGRY/SURPRISED四种情绪，
保留发送方、频道、会话、代次校验以及最近16个不同command ID的FIFO去重；issued_at和
expires_at可省略或提供但不参与时钟/有效期校验，不引入UTC依赖。绑定轮询与停止通知走
后台服务队列，本地RTC/媒体停止和状态发布先于停止HTTP通知。

本阶段不修改旧硬件UI渲染器、音频、按键、视频、电源、GPIO或分区，不迁入R2风格及场景
策略。旧状态卡片仍处理声纹与阶段bit 0-3，忽略新增情绪bit 4-7，后续UI阶段再接入。
AVDK、AOSL和RTSA构建输入保持原版本。

工程clean后AP/CP完整编译及组合打包通过，日志为/tmp/mybot-main-sdk-sync-build.log。
AP Flash为1,945,676字节、静态RAM为155,916字节、DTCM为35,744字节。
2026-10-16 15:26生成的build/bk7259/mybot/package/all-app.bin为3,453,024字节，
SHA256为496915e16230ae62bbcff6825c2389e417026c2f2fd0bf3e4fadd28dd92a42bd。

逐文件核对确认audio、key、video、usr_gpio及defconfig与旧硬件基线一致，LCD渲染器及
整个display目录无差异；生成配置仍为JD9855、ADC_KEY、VIDEO=y、5 fps。
AVDK仍固定f699c50f且工作树干净。此次仅完成SDK及构建验证，新的命令和会话行为仍待真机回归。

## 2026-10-16 旧硬件工艺灰与像素眼镜UI

SDK同步已先独立提交为父仓库5ebe531c258c3eab9e013118ac78e094464d4f7d及Solution
edfee2885defa9a4fc2c314aa836dc6689678510；本条仅记录其后的UI阶段。SDK仍为e7793eba，
AVDK仍固定f699c50f2ee72e7ca014c8f009e0d40f1eeb059a。新UI采用模拟器工艺灰/像素眼镜，
替换旧状态卡片；保留旧JD9855原生320x385、逻辑385x320及GPIO53供电/GPIO5复位/GPIO7
背光低有效的显示后端。

用户实物照片确认屏幕四角为明显圆弧；布局及原生预览需按保守圆角遮罩验证，确保终端文字、
配对码和眼镜主体不被遮挡。照片无法确定精确硬件半径，预览遮罩参数只作为设计/验收假设，
实际圆角效果以本次用户实机反馈为准。

完整23个表情均可本地选择，四行终端显示当前表情和真实运行状态，会话中显示阶段/声纹文本。
READY显示NEUTRAL；进入会话SURPRISED保持300 ms，另计进入过渡；LISTENING显示聆听，
THINKING连续三秒后PROCESSING，SPEAKING或无阶段时NEUTRAL。MCP HAPPY/SAD/ANGRY/
SURPRISED映射HAPPY/SAD/ANNOYED/SURPRISED，分别保持2000/3000/1200/800 ms；
开启动画时每次保持另加250 ms进入过渡。同名新命令重播，新命令替换旧前景，SDK原有
command ID去重不变。普通状态/声纹更新不重置保持计时，到期恢复最新自动表情；离开会话
清除计时，必要引导优先；恢复出厂保留旧板原流程，没有导入R2的5秒按键或复位阶段界面。
所有表情仅渲染LCD，不发送电机命令。

表情策略和LVGL继续由现有UI owner驱动，不新增UI线程、定时器或UTC依赖。旧defconfig
保留原语言/动画/浅色主题字段，LIGHT_THEME仅作兼容，新像素视图固定工艺灰；仅UI相关
Kconfig说明更新。四ADC按键、约2秒长按配网及原恢复出厂逻辑、双DMIC采集/AEC和GPIO20
功放控制、摄像头GPIO70/71/31/32、其他电源/GPIO、分区及AVDK均保留旧硬件配置。
本次验证结果：

- 画面等比缩放为346x302，位置(19,9)。终端9px、签名10px，四行Y为45/58/71/84，行高12px、行距13px；最长会话/声纹行185px，低于220px可用宽度。
- 配对码按用户确认的固定6位数字布局，固定Montserrat32字号，不再按长度切换字体。示例135790的实际字形范围108x35px。配网SSID和引导标签预留两行52px高度，避免旧小屏上的第二行裁切。
- 9/10px字体新增bitmap1757字节，ARM对象含字形/映射结构的新增只读符号共2913字节；无新增整帧缓冲、UI线程或运行期PCM/串口日志。
- Native LVGL以ASAN/UBSAN验证385x320中文、英文、无动画及640x480英文兼容配置，均通过36页面截图、23表情及六个动画阶段、phase/VP更新、文字范围、引导页和11轮生命周期检查。LSAN因宿主ptrace限制关闭，不宣称已完成泄漏检测。
- 圆角预览采用保守逻辑半径90px，不视为实测硬件参数。可见文字矩形四角及所有非背景像素均通过遮罩边界检查，包括过渡帧；中文/英文固定6位配对码、32字节SSID及眼镜主体完整位于安全区。
- 圆角总览 `/tmp/mybot-jd9855-pixel-ui.qwcFd4/rounded-r90-contact.png`，23表情总览同目录 `23-expressions-rounded-r90.png`；native harness为该目录的main.cc/CMakeLists.txt。主代理已查看配对和多页面圆角预览。
- 表情策略原生测试13组、462断言通过；当前文案所需140个UI字形覆盖检查通过。父仓库与Solution的 `git diff --check`通过。
- 标准AP/CP构建和组合打包通过，日志 `/tmp/mybot-main-pixel-ui-build.log`。2026-10-16 15:41生成 `build/bk7259/mybot/package/all-app.bin`，3,412,064字节，SHA256为 `7683238228313e21983b1c44ba05c126028a36e5010e65aa176cf5bad489c003`。
- AP Flash 1,904,716字节、RAM 158,284字节、DTCM 35,744字节；相比本次独立SDK提交时Flash减少40,960字节、静态RAM增加2368字节。静态RAM不代表LVGL及音视频并行运行时的堆峰值。
- 再次逐文件确认audio/key/video、AP defconfig/usr_gpio、CP与partitions相对edfee28没有差异；LCD电源/teardown及旋转/flush区块保持原样。AVDK f699c50f仍干净，SDK/AOSL摘要仍与版本锁一致，RTSA保持1.10.1 build1278380。
- 本次读取已选SDK的完整PORTING/EMBEDDED、LCD/platform/HTTPS/lifecycle合同与LVGL9.5对应API；旧边界脚本仍有已记录的版本/目录/旧显示规则不匹配，日志 `/tmp/mybot-main-sdk-sync-boundary.log`，不宣称其通过。

2026-10-16用户烧录本版旧硬件UI后反馈“测试通过”，据此将UI改动单独提交，并更新父仓库
的Solution依赖指针。SDK同步仍保留为此前的独立提交，AVDK版本不变，本次未推送。
长时间运行及同时音视频的资源峰值继续保留为专项验证项。
