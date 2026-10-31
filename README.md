# RTX 3060 GA106 2503：Windows EFI 算力解锁实验

RTX 3060 GA106 2503: Experimental Windows EFI Compute Unlock

[中英对照 / Bilingual](README.md) · [简体中文](README.zh-CN.md) · [English](README.en.md)

在一台 **X99-XD3 / RTX 3060 12GB（10de:2503）/ Windows 10** 上，已完成 USB 手动解锁、Windows 驱动交接和硬盘 EFI 自动解锁的实机验证。FP16/BF16 输入、FP32 输出的 GEMM 实测比未解锁提高约 **68%～86%**。

**这是单机验证的研究实现。附带 EFI 和安装器绑定本机硬件、AMI 显示驱动及 Windows ESP，其他电脑不能直接照搬。仅有相同的 2503 ID 不足以兼容。**

USB-triggered unlocking, Windows driver handoff, and automatic unlocking from the internal EFI System Partition have been verified on **one X99-XD3 / RTX 3060 12GB (10de:2503) / Windows 10 system**. Measured GEMM throughput with FP16/BF16 inputs and FP32 output increased by approximately **68%–86%** over the locked baseline.

**This is a research implementation validated on a single captured machine. The supplied EFI images and installer are bound to its hardware, AMI display driver, and Windows EFI System Partition (ESP). They are not drop-in tools for other PCs. A matching 2503 device ID alone does not establish compatibility.**

## 已验证环境 / Verified system

| 项目 | 实测配置 |
|---|---|
| GPU | RTX 3060 12GB，GA106，PCI 10de:2503，子系统 10de:2503 |
| 主板 / CPU | X99-XD3 / Xeon E5-2686 v4，单独显卡承担显示输出 |
| 系统 / 驱动 | Windows 10 19045 / NVIDIA 581.80 |
| VBIOS | 94.06.14.00.ad；PROM 哈希见 profile |
| GPU PCI / BAR0 | 0000:07:00.0 / 0xfa000000 |
| 测量 | PyTorch 2.11.0+cu128，CUDA 12.8，170 W |
| 实机进度 | v10 USB 核心成功；近一小时训练由所有者报告精度正常；v11 无 USB 自动启动成功 |

| Item | Verified configuration |
|---|---|
| GPU | RTX 3060 12GB, GA106, PCI 10de:2503, subsystem 10de:2503 |
| Motherboard / CPU | X99-XD3 / Xeon E5-2686 v4; the single discrete GPU also drives the display |
| OS / driver | Windows 10 19045 / NVIDIA 581.80 |
| VBIOS | 94.06.14.00.ad; see the profile for the PROM hash |
| GPU PCI / BAR0 | 0000:07:00.0 / 0xfa000000 |
| Measurement environment | PyTorch 2.11.0+cu128, CUDA 12.8, 170 W |
| Hardware validation | v10 USB core succeeded; the owner reported normal accuracy during nearly one hour of training; v11 automatic boot succeeded without a USB drive |

## 内容 / Package contents

- `source/`：实测 EFI 的对应 C 源码、兼容层、原核心、EDK2 头文件、测试及原本机脚本。
- `releases/usb-v10/UNLOCK.EFI`：15 秒内按 **U** 执行一次；不按则进入 Windows。
- `releases/auto-v11/AUTO.EFI`：本机硬盘自动版，5 秒内任意键跳过。
- `tools/`：相对路径重新构建、公开测试、固件准备、完整性校验及仅本机安装入口。
- `evidence/`：离线测试和脱敏实测数据。测试通过与实机验证分开记录。
- `firmware/`：固定来源和提取/生成脚本。**不附 NVIDIA 固件、PROM、AMI 镜像、Windows 启动文件或系统备份。**

- `source/`: corresponding C source for the tested EFI images, the compatibility layer, original core, EDK2 headers, tests, and original owner-specific scripts.
- `releases/usb-v10/UNLOCK.EFI`: press **U** within 15 seconds to execute once; otherwise it boots Windows.
- `releases/auto-v11/AUTO.EFI`: automatic internal-drive version for the captured machine; press any key within 5 seconds to skip.
- `tools/`: relative-path rebuilding, public tests, firmware preparation, integrity verification, and an owner-specific installation entry point.
- `evidence/`: offline test results and privacy-filtered hardware measurements. Simulation results and hardware validation are recorded separately.
- `firmware/`: pinned sources and extraction/generation scripts. **NVIDIA firmware, PROM dumps, AMI images, Windows boot files, and system backups are not included.**

## 先阅读 / Documentation

1. [方案与执行流程](docs/ARCHITECTURE.md)
2. [实测性能和范围](docs/RESULTS.md)
3. [构建及准备固件](docs/BUILD.md)
4. [本机安装和 USB 布局](docs/USAGE.md)
5. [恢复方案](docs/RECOVERY.md)
6. [适配另一台机器](docs/PORTING.md)
7. [测试与证据](docs/TESTING.md)
8. [上传 GitHub](docs/PUBLISHING.md)

运行 `python tools/verify_package.py` 可离线核对整个公开包，不会安装或执行解锁。开发者构建入口是 `sh tools/build.sh usb` / `sh tools/build.sh auto`，不是历史本机路径脚本。

1. [Architecture and execution flow](docs/ARCHITECTURE.md)
2. [Measured results and their scope](docs/RESULTS.md)
3. [Building and preparing firmware](docs/BUILD.md)
4. [Owner-specific installation and USB layout](docs/USAGE.md)
5. [Recovery](docs/RECOVERY.md)
6. [Porting to another machine](docs/PORTING.md)
7. [Tests and evidence](docs/TESTING.md)
8. [Publishing on GitHub](docs/PUBLISHING.md)

The detailed documents linked above are currently primarily in Chinese. This README is available in both languages.

Run `python tools/verify_package.py` to verify the public package offline; it does not install or execute the unlock. The public build entry points are `sh tools/build.sh usb` and `sh tools/build.sh auto`, rather than the historical scripts that depend on the original machine's directory layout.

## 原理和持久性 / How it works and persistence

在 Windows 驱动加载前，EFI 独占显卡，使用上游 610.43.03 FWSEC/V67 流程设置运行时选择器，完成一次 FLR，清理 DMA、恢复 VGA 转发，然后交给原 Windows Boot Manager。不会刷写显卡 VBIOS 或修改 NVIDIA Windows 驱动。硬盘安装会新增 EFI 文件、SYSTEM 确认任务和 NVRAM 启动项，并改变启动顺序。

解锁状态不是刷进显卡的永久改动。自动入口在符合条件的启动中重新执行；上次未确认成功会跳过。休眠、热重启、驱动升级及其他主板未在本项目中得到完整验证。算力提升只代表列出的 GEMM；训练速度取决于实际负载。

Before the Windows driver loads, the EFI application takes exclusive ownership of the GPU, uses the upstream 610.43.03 FWSEC/V67 flow to set runtime selectors, performs one PCIe Function Level Reset (FLR), cleans up DMA resources, restores VGA forwarding, and hands off to the original Windows Boot Manager. It does not flash the GPU VBIOS or patch the NVIDIA Windows driver. Internal-drive installation adds EFI files, a SYSTEM boot-confirmation task, and an NVRAM boot entry, and changes the boot order.

The unlock is a runtime state, not a permanent modification flashed into the GPU. The automatic entry executes again on eligible boots; it skips unlocking if the previous attempt was not confirmed successful. Hibernation, warm restarts, driver upgrades, and other motherboards have not been fully validated by this project. The reported uplift applies to the listed GEMM measurements; actual training speed depends on the workload.

## 卡住加载界面时恢复 / Recovery if boot appears stuck

如果卡在加载界面、下划线持续闪烁，**两分钟仍未进入 Windows**：

1. 长按电源键关机，移除 U 盘（如有）。
2. 断开主机外部电源，按住电源键约 **10 秒**，再等待约 **30 秒**。
3. 接电开机，在固件启动菜单选择原 **Windows Boot Manager**。
4. 回到 Windows 后，运行安装时生成的桌面 `DISABLE-GPU-AUTO.cmd`，允许管理员提示，关闭自动解锁。

自动入口也可在 5 秒窗口内按任意键跳过。上次未确认成功时，PENDING/LASTRUN 标记会保留，后续启动会跳过解锁；不要删除失败标记强行重试。断电通常清除运行时状态，但不能保证解决所有硬件异常。PE 和无法进入 BIOS 的情况见[完整恢复方案](docs/RECOVERY.md)。

If the loading screen or blinking underscore persists and **Windows has not started after two minutes**:

1. Hold the power button to shut down; remove the USB drive, if present.
2. Disconnect external power from the PC, hold its power button for about **10 seconds**, then wait about **30 seconds**.
3. Reconnect power, turn on the PC, and select the original **Windows Boot Manager** in the firmware boot menu.
4. Once back in Windows, run the desktop `DISABLE-GPU-AUTO.cmd` created during installation and accept the administrator prompt to disable automatic unlocking.

You can also press any key during the automatic entry's 5-second window to skip it. An unconfirmed previous attempt leaves PENDING/LASTRUN markers in place so later boots skip unlocking; do not delete failure markers to force another attempt. Removing power usually clears the runtime state, but cannot be guaranteed to resolve every hardware fault. For PE recovery or an inaccessible BIOS screen, see the [full recovery guide](docs/RECOVERY.md), currently in Chinese.

## 来源与许可证 / Attribution and licensing

主要基础为 [impacifist/Windows-GPU-UNLOCK](https://github.com/impacifist/Windows-GPU-UNLOCK)、[Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock) 和 [JustAResearcher/gpu-unlock-ada](https://github.com/JustAResearcher/gpu-unlock-ada)。保留固定提交、原作者与许可证，详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。本研究扩展按 GPL-2.0-only 提供，EDK2 头文件沿用其许可证；不以本仓库许可证重新授权 NVIDIA 固件。

The main foundations are [impacifist/Windows-GPU-UNLOCK](https://github.com/impacifist/Windows-GPU-UNLOCK), [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock), and [JustAResearcher/gpu-unlock-ada](https://github.com/JustAResearcher/gpu-unlock-ada). Pinned revisions, original authorship, and license notices are retained; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). This research extension is provided under GPL-2.0-only. EDK2 headers retain their own license, and this repository's license does not relicense NVIDIA firmware.
