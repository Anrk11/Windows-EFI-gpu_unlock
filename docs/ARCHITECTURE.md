# 方案与执行流程

## 为什么不是 Windows 内直接运行

这套已验证流程依赖在操作系统显卡驱动接管之前独占设备、运行固件和复位。Windows 里的程序负责准备、安装、只读确认和算力对比；实际解锁发生在 EFI。WSL 不提供这条启动前硬件控制路径。这里没有声称在 Windows 驱动运行中热解锁可行。

## 本机适配的关键

1. 固定 GPU、子系统、PCI 路径、BAR0、显存、PROM 哈希和入口状态。
2. 核对实际 AMI 显示驱动的代码、载入基址和显示拓扑，再解除固件显示驱动。
3. 保持 BME 关闭进行资源及 DMA 映射/释放预演；执行前核对五个固件的大小、完整读取及 SHA-256。
4. EXCLUSIVE 占有准确设备，执行原核心 FWSEC/V67，写 SS0=0x88888888、SS1=8，一次 FLR 后重新读取验证。
5. 检查 GFW、权限、WPR、停机状态和 DMA 清理；恢复原 VGA 转发 0x18，属性读回 0x318。此前缺少 VGA 转发导致了 Windows 交接故障。
6. 直接启动预加载的原 Windows Boot Manager，不在核心执行后重新连接 GOP。

执行前拒绝不符的环境；核心执行后的异常走保留必要资源并停机的路径，不能保证所有失败都自动回到 Windows。

## 自动启动状态机

- 从固定 Windows ESP 的 `EFI/Codex2503/AUTO.EFI` 加载，不能从 USB 运行自动版。
- 5 秒内任意键跳过；不按键则尝试运行一次。
- 先持久写入 `PENDING.TXT`，过程记录在 `LASTRUN.TXT`。
- 已有 PENDING、LASTRUN、DISABLED 或读取异常，则跳过解锁进入 Windows。
- 开机后 SYSTEM 确认任务核对当前启动时间、完整 v11 日志、EFI 哈希、准确 PnP 设备问题码、NVML 和驱动版本。
- 通过后先归档并核对日志，再依次删除 LASTRUN、PENDING，放行下次自动启动。失败保留状态，不反复运行。
- 确认任务不运行算力测试；性能验证来自独立的匹配 GEMM。

## 文件与源码入口

`source/research/2503-handoff/main.c` 为实际入口；`core_efi.h` 负责资产和核心生命周期；`core_bridge.c`、`transaction.c`、`dma_probe.c` 负责约束与交接；`auto_policy.h` 负责自动触发；`windows.h` 负责原 Windows 加载。`source/internal-boot/unlock/vendor/` 保留原解锁核心。`core_model.h` 仅用于主机模拟，不能编为实际固件。

`GU_EXPERIMENTAL_3090` 是沿用上游编译分支的名称。本机选用独立 `GU_RESEARCH_2503_FIRMWARE` 配置，不把 2503 冒充 2504 或 3090。五个固件中的 ga102 名称是沿用固定上游 GA10x 资产，不应擅自重命名。
