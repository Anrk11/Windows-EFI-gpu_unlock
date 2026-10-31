# RTX 3060 GA106 2503：Windows EFI 算力解锁实验

[中英对照 / Bilingual](README.md) · [简体中文](README.zh-CN.md) · [English](README.en.md)

在一台 **X99-XD3 / RTX 3060 12GB（10de:2503）/ Windows 10** 上，已完成 USB 手动解锁、Windows 驱动交接和硬盘 EFI 自动解锁的实机验证。FP16/BF16 输入、FP32 输出的 GEMM 实测比未解锁提高约 **68%～86%**。

**这是单机验证的研究实现。附带 EFI 和安装器绑定本机硬件、AMI 显示驱动及 Windows ESP，其他电脑不能直接照搬。仅有相同的 2503 ID 不足以兼容。**

## 已验证环境

| 项目 | 实测配置 |
|---|---|
| GPU | RTX 3060 12GB，GA106，PCI 10de:2503，子系统 10de:2503 |
| 主板 / CPU | X99-XD3 / Xeon E5-2686 v4，单独显卡承担显示输出 |
| 系统 / 驱动 | Windows 10 19045 / NVIDIA 581.80 |
| VBIOS | 94.06.14.00.ad；PROM 哈希见 profile |
| GPU PCI / BAR0 | 0000:07:00.0 / 0xfa000000 |
| 测量 | PyTorch 2.11.0+cu128，CUDA 12.8，170 W |
| 实机进度 | v10 USB 核心成功；近一小时训练由所有者报告精度正常；v11 无 USB 自动启动成功 |

## 内容

- `source/`：实测 EFI 的对应 C 源码、兼容层、原核心、EDK2 头文件、测试及原本机脚本。
- `releases/usb-v10/UNLOCK.EFI`：15 秒内按 **U** 执行一次；不按则进入 Windows。
- `releases/auto-v11/AUTO.EFI`：本机硬盘自动版，5 秒内任意键跳过。
- `tools/`：相对路径重新构建、公开测试、固件准备、完整性校验及仅本机安装入口。
- `evidence/`：离线测试和脱敏实测数据。测试通过与实机验证分开记录。
- `firmware/`：固定来源和提取/生成脚本。**不附 NVIDIA 固件、PROM、AMI 镜像、Windows 启动文件或系统备份。**

## 先阅读

1. [方案与执行流程](docs/ARCHITECTURE.md)
2. [实测性能和范围](docs/RESULTS.md)
3. [构建及准备固件](docs/BUILD.md)
4. [本机安装和 USB 布局](docs/USAGE.md)
5. [恢复方案](docs/RECOVERY.md)
6. [适配另一台机器](docs/PORTING.md)
7. [测试与证据](docs/TESTING.md)
8. [上传 GitHub](docs/PUBLISHING.md)

运行 `python tools/verify_package.py` 可离线核对整个公开包，不会安装或执行解锁。开发者构建入口是 `sh tools/build.sh usb` / `sh tools/build.sh auto`，不是历史本机路径脚本。

## 原理和持久性

在 Windows 驱动加载前，EFI 独占显卡，使用上游 610.43.03 FWSEC/V67 流程设置运行时选择器，完成一次 FLR，清理 DMA、恢复 VGA 转发，然后交给原 Windows Boot Manager。不会刷写显卡 VBIOS 或修改 NVIDIA Windows 驱动。硬盘安装会新增 EFI 文件、SYSTEM 确认任务和 NVRAM 启动项，并改变启动顺序。

解锁状态不是刷进显卡的永久改动。自动入口在符合条件的启动中重新执行；上次未确认成功会跳过。休眠、热重启、驱动升级及其他主板未在本项目中得到完整验证。算力提升只代表列出的 GEMM；训练速度取决于实际负载。

## 卡住加载界面时恢复

如果卡在加载界面、下划线持续闪烁，**两分钟仍未进入 Windows**：

1. 长按电源键关机，移除 U 盘（如有）。
2. 断开主机外部电源，按住电源键约 **10 秒**，再等待约 **30 秒**。
3. 接电开机，在固件启动菜单选择原 **Windows Boot Manager**。
4. 回到 Windows 后，运行安装时生成的桌面 `DISABLE-GPU-AUTO.cmd`，允许管理员提示，关闭自动解锁。

自动入口也可在 5 秒窗口内按任意键跳过。上次未确认成功时，PENDING/LASTRUN 标记会保留，后续启动会跳过解锁；不要删除失败标记强行重试。断电通常清除运行时状态，但不能保证解决所有硬件异常。PE 和无法进入 BIOS 的情况见[完整恢复方案](docs/RECOVERY.md)。

## 来源与许可证

主要基础为 [impacifist/Windows-GPU-UNLOCK](https://github.com/impacifist/Windows-GPU-UNLOCK)、[Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock) 和 [JustAResearcher/gpu-unlock-ada](https://github.com/JustAResearcher/gpu-unlock-ada)。保留固定提交、原作者与许可证，详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。本研究扩展按 GPL-2.0-only 提供，EDK2 头文件沿用其许可证；不以本仓库许可证重新授权 NVIDIA 固件。
