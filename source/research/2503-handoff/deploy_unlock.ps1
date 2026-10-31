param([string]$DriveLetter='G')
$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$workspace=Split-Path $repo -Parent
$capture=Join-Path $workspace 'rtx3060-capture-20261030-141202'
$build=Join-Path $PSScriptRoot 'build-unlock'
$manifest=Get-Content -LiteralPath (Join-Path $build 'manifest.json') -Raw | ConvertFrom-Json
if($manifest.TestVersion -ne 10 -or !$manifest.CanUnlock -or $manifest.HardwareTested -or $manifest.FullAdapterScenariosPassed -ne 15 -or $manifest.BootScenariosPassed -ne 183 -or $manifest.CoreScenariosPassed -ne 252){throw 'Build or test gates are incomplete'}
foreach($source in $manifest.Sources.PSObject.Properties){
 if((Get-FileHash -LiteralPath (Join-Path $repo $source.Name)).Hash.ToLowerInvariant() -cne $source.Value){throw "Source changed after tests/build: $($source.Name)"}
}
$image=Join-Path $build 'UNLOCK.EFI'
if((Get-FileHash -LiteralPath $image).Hash.ToLowerInvariant() -cne $manifest.EfiSha256){throw 'EFI build hash mismatch'}
$volume=Get-Volume -DriveLetter $DriveLetter
$disk=Get-Partition -DriveLetter $DriveLetter | Get-Disk
if($volume.FileSystem -ne 'FAT32' -or $volume.UniqueId -ne '\\?\Volume{46401cc1-4223-11f1-8ae6-5c7dae3b4a0b}\' -or $disk.BusType -ne 'USB' -or $disk.FriendlyName -ne 'Lexar USB Flash Drive'){throw 'Unexpected USB identity'}
$usbRoot=$DriveLetter+':\'
$active=Join-Path $usbRoot 'EFI\BOOT\BOOTX64.EFI'
$backup=Join-Path $usbRoot 'EFI\2503-HANDOFF\HANDOFF-v9.EFI'
$goodHash='d5eac53b055f6637c840c571d319c54af3793f80a979f4791a8fd6f087853b3a'
if((Get-FileHash -LiteralPath $backup).Hash.ToLowerInvariant() -cne $goodHash){throw 'Verified recovery image is missing or changed'}
if((Get-FileHash -LiteralPath $active).Hash.ToLowerInvariant() -cne $goodHash){throw 'Active boot image changed; inspect before replacing it'}
if(Test-Path -LiteralPath (Join-Path $usbRoot '2503-UNLOCK.TXT')){throw 'An unlock log exists; preserve and inspect it before another deployment'}
$archive=Join-Path $capture ('unlock-10-deployment-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $archive | Out-Null
$prior=Join-Path $usbRoot 'EFI\2503-HANDOFF\installation.json'
Copy-Item -LiteralPath $prior -Destination (Join-Path $archive 'prior-v9-installation.json')
Copy-Item -LiteralPath $active -Destination (Join-Path $archive 'prior-v9.EFI')
if((Get-FileHash -LiteralPath (Join-Path $archive 'prior-v9.EFI')).Hash.ToLowerInvariant() -cne $goodHash){throw 'Recovery archive verification failed'}
$target=Join-Path $usbRoot 'EFI\2503-UNLOCK'
$targetAssets=Join-Path $target 'ASSETS'
New-Item -ItemType Directory -Path $targetAssets -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $archive 'prior-v9-installation.json') -Destination (Join-Path $target 'prior-v9-installation.json')
foreach($asset in $manifest.Assets){
 $source=Join-Path (Join-Path $workspace 'rtx3060-unlock-assets\assets') $asset.Name
 if((Get-Item -LiteralPath $source).Length -ne $asset.Bytes -or (Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant() -cne $asset.Sha256){throw "Asset mismatch: $($asset.Name)"}
 $destination=Join-Path $targetAssets $asset.Name
 Copy-Item -LiteralPath $source -Destination $destination
 if((Get-Item -LiteralPath $destination).Length -ne $asset.Bytes -or (Get-FileHash -LiteralPath $destination).Hash.ToLowerInvariant() -cne $asset.Sha256){throw "USB asset mismatch: $($asset.Name)"}
}
foreach($name in @('restore-v9.ps1','verify-win.ps1')){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $target $name)
 if((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $target $name)).Hash){throw 'Helper copy failed'}
}
foreach($name in @('RESTORE-WINDOWS.cmd','VERIFY-WINDOWS.cmd')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $usbRoot $name)}
Copy-Item -LiteralPath (Join-Path $build 'manifest.json') -Destination (Join-Path $target 'manifest-v10.json')
Copy-Item -LiteralPath (Join-Path $build 'pe-verification.json') -Destination (Join-Path $target 'pe-verification.json')
Copy-Item -LiteralPath $image -Destination (Join-Path $target 'UNLOCK-v10.EFI')
if((Get-FileHash -LiteralPath (Join-Path $target 'UNLOCK-v10.EFI')).Hash.ToLowerInvariant() -cne $manifest.EfiSha256){throw 'USB staged EFI hash mismatch'}
$instructions=@'
RTX 3060 2503 实验解锁版（第 10 版）

1. 保存工作，重启后选择 U 盘的 UEFI 启动项。
2. 启动菜单出现后 15 秒内按 U（现在不是 T）。不按 U 会直接进入 Windows，不执行解锁。
3. 程序会运行实际解锁核心、写入解锁值并复位一次显卡；显示会暂时中断，通过后自动进入 Windows。
4. 两分钟后仍没有 Windows：长按电源关机，拔掉 U 盘；断开电源，按电源键十秒，等三十秒，接电后从 Windows Boot Manager 启动。
5. 恢复进入 Windows 后，插回 U 盘，双击 RESTORE-WINDOWS.cmd 可恢复已验证的第 9 版。无需修改 BIOS 或刷显卡固件。

本版已通过离线检查，尚未在这张显卡上执行实际解锁。正常进入 Windows 后仍需检查日志和对比算力，不能仅凭进入系统认定已经解锁。
解锁日志：U 盘根目录 2503-UNLOCK.TXT。已有该日志时程序拒绝再次执行，避免重复实验。
只读检查：VERIFY-WINDOWS.cmd。不会重启或卸载显卡驱动。
恢复备份：EFI\2503-HANDOFF\HANDOFF-v9.EFI。之前通过的交接日志仍保留。
'@
$instructions | Set-Content -LiteralPath (Join-Path $usbRoot '2503-解锁说明.txt') -Encoding UTF8
try {
 Copy-Item -LiteralPath (Join-Path $target 'UNLOCK-v10.EFI') -Destination $active -Force
 if((Get-FileHash -LiteralPath $active).Hash.ToLowerInvariant() -cne $manifest.EfiSha256){throw 'Active EFI verification failed'}
} catch {
 Copy-Item -LiteralPath $backup -Destination $active -Force
 throw
}
$installed=[ordered]@{InstalledUtc=[DateTime]::UtcNow.ToString('o');TestVersion=10;CanUnlock=$true;HardwareTested=$false;UnlockExecuted=$false;EfiSha256=$manifest.EfiSha256;ManualActivationKey='U';BootScenariosPassed=183;CoreScenariosPassed=252;FullAdapterScenariosPassed=15;RecoveryVersion=9;RecoverySha256=$goodHash;Archive=$archive;DeploymentRequestedByUser=$true;WindowsDeviceRestartEnabled=$false;Log=(Join-Path $usbRoot '2503-UNLOCK.TXT')}
$installed | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $target 'installation.json') -Encoding UTF8
$installed | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $prior -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $target 'installation.json') -Destination (Join-Path $archive 'installation.json')
foreach($name in @('manifest.json','frontend-verification.json','pe-verification.json','boot-results.txt','frontend-results.txt')){Copy-Item -LiteralPath (Join-Path $build $name) -Destination (Join-Path $archive $name)}
$installed | ConvertTo-Json -Depth 4
