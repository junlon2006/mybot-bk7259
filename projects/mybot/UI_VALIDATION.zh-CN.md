# BK7259 固件验证

**[English](UI_VALIDATION.md) | 简体中文**

## 范围

本页记录 `main` Robot V2 固件当前可复现的检查和剩余发布阻塞项，替代原先按日期累积的
开发日志。旧的中间诊断可在 Git 历史中查询。

以下状态于 2026-10-22 复核。本次文档更新没有执行目标板测试或完整固件重编译。

## 当前源码与主机检查

固定源码集通过：

```bash
python3 scripts/check_sources.py
```

该检查验证：

- AVDK 和 Solution 的 gitlink 及已跟踪文件状态。
- 补丁摘要以及 MyBot/AOSL 内容摘要。
- RTSA 头文件和静态库摘要。
- 产品源码 include 边界。

构建准备测试：

```bash
(cd scripts && python3 -m unittest test_build_all.py test_vendor_workspace.py)
```

当前结果：16 项测试通过。

产品主机测试：

```bash
for script in projects/mybot/tests/*/run.sh; do
  bash "$script"
done
```

测试覆盖：

- 表情和 LCD indicator 策略：13 组、462 条断言。
- HTTPS 重试、超时、EOF、socket 错误隔离和时钟回绕。
- 恢复出厂持久化、启动续做、删除和网络故障。
- 恢复所有权、SDK/网络停止门禁和 S2 独立配网。
- Wi-Fi 管理清理、取消、重连、解挂、配网门户和存储故障。

当前主机测试全部通过，但不能替代目标硬件验证。

## 固件构建

固件重编译命令：

```bash
python3 scripts/build_all.py
```

该命令生成中英文 AP/CP 镜像并校验生成的服务地址。本次 SDK 同步已重新构建两种语言，
目标端时序和资源高水位仍需实际测量。

## 发布阻塞项

以下项目需要代码或契约修改：

1. **证书有效期：** 适配层启用了证书链和主机名校验，但当前 MbedTLS 配置没有启用
   `MBEDTLS_HAVE_TIME_DATE`。必须恢复证书过期和未生效检查并补充测试。
2. **凭据存储：** Wi-Fi 和设备凭据保存在未静态加密的 EasyFlash 中。
3. **RTSA 分发：** 仓库包含专有 RTSA 输入，但没有独立再分发授权记录。
4. **目标测量：** 最终代码和资源确定后，需重新测量固件 Flash、OTA、PSRAM 峰值和栈高水位。

## 目标板检查

量产前需要：

- 冷启动组合镜像 `all-app.bin`，确认两个核正常运行。
- 验证 Robot V2 ADC 按键窗口及全部短按、长按功能。
- 验证双 DMIC 采集、硬件 AEC、音量键和 RTC 双向音频。
- 验证配网、取消、重连和掉电恢复。
- 验证摄像头识别、H.264、帧率、码率调整、IDR 和重复视频会话。
- 验证 JD9855 方向、颜色、圆角安全区、中英文和全部 23 个表情。
- 在音频、视频、界面、HTTP 和 RTC 并发时测量栈、堆、PSRAM 和时序余量。
- 验证实际服务的证书链、主机名、有效期和超时。
- 执行长时间运行和看门狗恢复测试。

主机测试和固件编译不能证明目标硬件行为。
