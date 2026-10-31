# 使用与本机安装

## 发布的适用范围

附带二进制和 `tools/install_owner.ps1` 仅用于 README 的那台已验证机器。ESP GUID 被有意保留为固定配置的一部分，不是其他电脑应复制的分区标识。其他机器先阅读 PORTING。不要因为同为 3060/2503 就把 EFI 设为默认启动。

## 原机 USB 手动版的文件布局

使用可由固件 UEFI 启动的 FAT32 USB 分区；MBR/FAT32 在本机实测可用。不要格式化已有数据来试验。

```text
EFI/BOOT/BOOTX64.EFI                 ← releases/usb-v10/UNLOCK.EFI
EFI/2503-UNLOCK/ASSETS/<五个文件>      ← 本地准备的固定固件
2503-UNLOCK.TXT                     ← 程序生成；已有则拒绝再次执行
```

从 USB UEFI 启动，15 秒内按 U 执行核心；不按则进入 Windows。执行中显示可能中断。若两分钟仍未进系统，按 RECOVERY 恢复。完成后保留并核对本次日志、Windows 驱动状态，再用相同条件测试算力。历史日志必须先归档，不能通过删除失败日志盲目重试。原本机部署脚本包含固定 Lexar USB 身份及旧恢复镜像要求，是历史安装代码，不是创建任意 USB 的工具。

## 原机硬盘自动版

Secure Boot 必须已关闭，Windows 正常，C 盘已完全解密且保护关闭，快速启动已关闭。安装器核对原机 ESP 身份、原 Windows Boot Manager、完整镜像/源码和五个资产哈希、64 MiB 空间余量；不自动修改这些前提。先手动验证核心和代表性负载，再考虑自启。

在原机管理员 PowerShell 中，固件准备完成后：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\tools\install_owner.ps1" -AssetsDirectory ".\local\firmware"
```

公开安装入口以仓库相对路径找到实测 v11 镜像，资产路径可指定；仍执行原本机的其余所有安装检查。**当前已安装的原机无需再次运行；存在 installation.json 时会拒绝重装。**

安装会在原 ESP 新建 `EFI\Codex2503`，新增自启项并排第一，原 Windows Boot Manager 保留第二；备份原状态并注册 SYSTEM 启动确认任务。它不会替换微软引导、刷 VBIOS、卸载显卡驱动或主动重启。

5 秒内任意键跳过；正常进入 Windows 后查看 `C:\ProgramData\Codex2503AutoBoot\last-confirmation.json`。失败保留标记，下次跳过。恢复入口在安装时生成的桌面 `DISABLE-GPU-AUTO.cmd`。默认记录仍用原 `Codex2503AutoBoot` 名称以兼容已部署版本。

## 算力与数值验证

原 benchmark 固定本机 PCI 身份，GPU 忙/热时拒绝；需要兼容的 PyTorch/CUDA 环境：

```powershell
python .\source\research\2503-handoff\benchmark_2503.py --label after-unlock-attempt --output .\local\after-unlock-gemm.json
```

命令只运行短 GEMM，不改变显卡设置。需要在未解锁状态用 `--label stock` 单独保存本机基线；不能拿另一台机器或不同精度模式的分数作解锁证据。
