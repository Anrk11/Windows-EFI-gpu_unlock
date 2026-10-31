# RTX 3060 GA106 2503: Experimental Windows EFI Compute Unlock

[中英对照 / Bilingual](README.md) · [简体中文](README.zh-CN.md) · [English](README.en.md)

USB-triggered unlocking, Windows driver handoff, and automatic unlocking from the internal EFI System Partition have been verified on **one X99-XD3 / RTX 3060 12GB (10de:2503) / Windows 10 system**. Measured GEMM throughput with FP16/BF16 inputs and FP32 output increased by approximately **68%–86%** over the locked baseline.

**This is a research implementation validated on a single captured machine. The supplied EFI images and installer are bound to its hardware, AMI display driver, and Windows EFI System Partition (ESP). They are not drop-in tools for other PCs. A matching 2503 device ID alone does not establish compatibility.**

## Verified system

| Item | Verified configuration |
|---|---|
| GPU | RTX 3060 12GB, GA106, PCI 10de:2503, subsystem 10de:2503 |
| Motherboard / CPU | X99-XD3 / Xeon E5-2686 v4; the single discrete GPU also drives the display |
| OS / driver | Windows 10 19045 / NVIDIA 581.80 |
| VBIOS | 94.06.14.00.ad; see the profile for the PROM hash |
| GPU PCI / BAR0 | 0000:07:00.0 / 0xfa000000 |
| Measurement environment | PyTorch 2.11.0+cu128, CUDA 12.8, 170 W |
| Hardware validation | v10 USB core succeeded; the owner reported normal accuracy during nearly one hour of training; v11 automatic boot succeeded without a USB drive |

## Package contents

- `source/`: corresponding C source for the tested EFI images, the compatibility layer, original core, EDK2 headers, tests, and original owner-specific scripts.
- `releases/usb-v10/UNLOCK.EFI`: press **U** within 15 seconds to execute once; otherwise it boots Windows.
- `releases/auto-v11/AUTO.EFI`: automatic internal-drive version for the captured machine; press any key within 5 seconds to skip.
- `tools/`: relative-path rebuilding, public tests, firmware preparation, integrity verification, and an owner-specific installation entry point.
- `evidence/`: offline test results and privacy-filtered hardware measurements. Simulation results and hardware validation are recorded separately.
- `firmware/`: pinned sources and extraction/generation scripts. **NVIDIA firmware, PROM dumps, AMI images, Windows boot files, and system backups are not included.**

## Documentation

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

## How it works and persistence

Before the Windows driver loads, the EFI application takes exclusive ownership of the GPU, uses the upstream 610.43.03 FWSEC/V67 flow to set runtime selectors, performs one PCIe Function Level Reset (FLR), cleans up DMA resources, restores VGA forwarding, and hands off to the original Windows Boot Manager. It does not flash the GPU VBIOS or patch the NVIDIA Windows driver. Internal-drive installation adds EFI files, a SYSTEM boot-confirmation task, and an NVRAM boot entry, and changes the boot order.

The unlock is a runtime state, not a permanent modification flashed into the GPU. The automatic entry executes again on eligible boots; it skips unlocking if the previous attempt was not confirmed successful. Hibernation, warm restarts, driver upgrades, and other motherboards have not been fully validated by this project. The reported uplift applies to the listed GEMM measurements; actual training speed depends on the workload.

## Recovery if boot appears stuck

If the loading screen or blinking underscore persists and **Windows has not started after two minutes**:

1. Hold the power button to shut down; remove the USB drive, if present.
2. Disconnect external power from the PC, hold its power button for about **10 seconds**, then wait about **30 seconds**.
3. Reconnect power, turn on the PC, and select the original **Windows Boot Manager** in the firmware boot menu.
4. Once back in Windows, run the desktop `DISABLE-GPU-AUTO.cmd` created during installation and accept the administrator prompt to disable automatic unlocking.

You can also press any key during the automatic entry's 5-second window to skip it. An unconfirmed previous attempt leaves PENDING/LASTRUN markers in place so later boots skip unlocking; do not delete failure markers to force another attempt. Removing power usually clears the runtime state, but cannot be guaranteed to resolve every hardware fault. For PE recovery or an inaccessible BIOS screen, see the [full recovery guide](docs/RECOVERY.md), currently in Chinese.

## Attribution and licensing

The main foundations are [impacifist/Windows-GPU-UNLOCK](https://github.com/impacifist/Windows-GPU-UNLOCK), [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock), and [JustAResearcher/gpu-unlock-ada](https://github.com/JustAResearcher/gpu-unlock-ada). Pinned revisions, original authorship, and license notices are retained; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). This research extension is provided under GPL-2.0-only. EDK2 headers retain their own license, and this repository's license does not relicense NVIDIA firmware.
