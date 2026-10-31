# 适配另一台 2503 机器

目前兼容范围是一台已捕获的 X99-XD3。2503/2504 都是 PCI 设备 ID，ID 相同不代表 ROM、GPU 修订、板卡、固件显示驱动或 PCI 资源布局相同。本项目不声称覆盖所有 2503，也没有提供已经验证的通用安装器。

## 必须重新获取并审核的资料

- GPU/子系统 ID、芯片、显存、PCI 拓扑和路径、BAR 地址、完整配置快照。
- PROM 哈希、VBIOS 版本、FWSEC 布局、权限及冷态选择器。
- 显示驱动绑定、GOP、子句柄、AMI 驱动镜像的来源、代码哈希、载入范围及 Stop/Start 行为。
- 实际 Windows ESP GUID、启动文件和原 Windows Boot Manager、加密与 Secure Boot 状态。
- 驱动版本、Windows 问题码、原始算力基线。

关键约束在 `main.c`、`core_firmware_profile.h`、`core_bridge.c`、`core_efi.h`、`windows.h`、`auto_policy.h`、`source/internal-boot/boot/platform-config.h` 和安装/确认脚本中。AMI 代码及基址也会检查；更改 GUID、改设备 ID 或删掉某一个拒绝条件不能完成适配。

## 验证顺序

只读采集 → 显示驱动解除与恢复 → DMA 生命周期预演 → 原 Windows 显示交接 → RAM 模拟失败分支 → 手动 USB 核心 → 本次日志与驱动状态 → 相同配置算力及数值校验 → 代表性训练稳定性 → 自动入口与失败跳过。

每一阶段都要有新的当前启动证据。历史日志、成功进入系统、模拟测试通过和性能提升是不同结论。每次修改配置都生成新构建/校验清单；原实测 EFI 的成功不会自动转移到修改后的二进制。

未公开机器 PROM、AMI 二进制及完整原始采集；完整主机模拟需要自行提供匹配且获授权的私有 fixture。仓库保留对应测试源码及既有结果。没有 fixture 仍可以编译和运行无需它们的公开策略测试。
