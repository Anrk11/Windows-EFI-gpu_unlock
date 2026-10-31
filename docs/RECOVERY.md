# 恢复方案

## 能看到启动菜单

自动入口的 5 秒窗口按任意键，跳过本次解锁。也可在主板启动菜单选择原 **Windows Boot Manager**。自动程序独立存放，没有覆盖微软启动文件。

## 闪烁下划线、显示未恢复或两分钟仍未进入 Windows

不要继续反复启动解锁。长按电源关闭，移除 USB，断开主机外部电源，按电源键约十秒，等待约三十秒后重新接电。从原 Windows Boot Manager 启动。断电通常清除这套运行时状态，但不能承诺所有硬件异常都由断电解决。

自动版上次未确认时会保留 PENDING/LASTRUN，下一次入口会跳过。**先保留失败日志，不要删除标记强行重试。**

## 已回到 Windows

双击安装时创建的 `DISABLE-GPU-AUTO.cmd`，允许管理员提示。它恢复 Windows Boot Manager 为第一项，并尝试写禁用标记；不删除分区、不刷显卡、不自动重启。也可从管理员 PowerShell 运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\ProgramData\Codex2503AutoBoot\auto_restore.ps1"
```

查看 `C:\ProgramData\Codex2503AutoBoot\last-confirmation.json` 和 `history`。失败原日志在 ESP 的 `EFI\Codex2503\LASTRUN.TXT`。

## 只能进入 PE

桌面 CMD 调用的是正常系统里的固定路径，**不能保证在 PE 直接运行有效**：PE 的 C 盘、BCD 存储与固件环境可能不同。

优先在 BIOS 启动菜单直接选原 Windows Boot Manager。若仍需在 PE 处理，先用磁盘工具确认真正的 FAT32 ESP 和 Windows 分区，再给 ESP 分配临时盘符，例如 S。保留 `EFI\Microsoft\Boot`，在实际存在的 `S:\EFI\Codex2503` 目录创建 `DISABLED.TXT`，使下次自动入口跳过。不要格式化 ESP，也不要盲目执行重建 BCD 或删除启动项。若微软引导本身损坏，需要按实际磁盘布局修复，不能直接套用本机分区号。

## 连 BIOS 画面都没有

先彻底断电恢复，再检查显示器输入和连接。该 EFI 只有在固件选择并加载它后才会运行；无法看到固件设置时不能据此断定是同一个原因。如果仍无法 POST，按主板手册的硬件排查流程处理，必要时清 CMOS；清 CMOS 会改变 BIOS 设置，应先了解主板操作方法。本项目不提供无条件恢复保证。
