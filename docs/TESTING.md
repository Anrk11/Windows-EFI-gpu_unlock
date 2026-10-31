# 测试与证据

## 无固件、无显卡访问的公开检查

```sh
python tools/verify_package.py
sh tools/test_public.sh
```

Windows PowerShell 5.1：

```powershell
powershell.exe -NoProfile -File .\source\research\2503-handoff\test_auto_confirm.ps1
powershell.exe -NoProfile -File .\source\research\2503-handoff\test_auto_bcd.ps1
```

公开 shell 测试运行 21 个自动触发/标记场景和 7 个 Windows 加载场景；PowerShell 运行 18 个确认策略和 5 个中英文 BCD 解析场景，不执行 BCD 修改。

## 原完整模拟记录

`evidence/offline` 保留原测试输出：183 个启动交接场景、252 个核心/交接场景、14 个 DMA 端口场景；USB 实际适配层与原核心联跑 15 个场景；自动版共 47 个场景（21 策略 + 7 Windows + 15 完整适配 + 4 来源拒绝）。这些测试使用 RAM 服务，不访问硬件。

完整重跑需要私有 PROM/AMI/采集日志和固定固件；对应入口在 `source/research/2503-handoff/test*.sh`。`test_auto_frontend.sh <capture> <work>` 与 `test_core_frontend.sh <capture> <work>` 对捕获文件、资产和派生 PCI fixture 有具体路径要求，见脚本和 `prepare_core_fixture.py`。未提供这些 fixture，因此不能把公开测试的 28 个场景说成已重跑全部原模拟。

本次整理时还从公开源码目录出发，使用原机保留在仓库外的私有 fixture，重新运行了自动版完整 47 个场景，全部通过；本次输出另存 `evidence/package-validation/auto-fixture-*`。这不意味着仅克隆公开仓库就能运行含私有输入的部分。

v11 与 v10 的 14 个核心/端口对象比较排除且仅排除 COFF 头部构建时间字段，其余字节相同；原 manifest 保留比较摘要。包整理时另外从对应源码重新构建 v10/v11 并核对实测二进制，记录见 `evidence/package-validation`。

## 实机证据

`evidence/measurements` 包含未解锁、USB 解锁、训练后和自动解锁 GEMM 原测量；自动确认摘要去掉账户路径和 PnP 实例。原始 EFI 日志、系统备份、PROM 和 AMI 镜像未公开，仅保留关键日志哈希及结果摘要。一次自动启动通过和约一小时训练不构成全部机器、所有驱动或长期稳定性的证明。
