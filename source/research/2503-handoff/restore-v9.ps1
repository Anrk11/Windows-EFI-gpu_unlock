param()
$ErrorActionPreference='Stop'
$drive=(Split-Path -Qualifier $PSScriptRoot).TrimEnd(':')
$volume=Get-Volume -DriveLetter $drive
if($volume.FileSystem -ne 'FAT32' -or $volume.UniqueId -ne '\\?\Volume{46401cc1-4223-11f1-8ae6-5c7dae3b4a0b}\'){throw 'Unexpected USB volume. Nothing changed.'}
$usbRoot=$drive+':\'
$backup=Join-Path $usbRoot 'EFI\2503-HANDOFF\HANDOFF-v9.EFI'
$active=Join-Path $usbRoot 'EFI\BOOT\BOOTX64.EFI'
$expected='D5EAC53B055F6637C840C571D319C54AF3793F80A979F4791A8FD6F087853B3A'
if((Get-FileHash -LiteralPath $backup).Hash -ne $expected){throw 'Version 9 backup does not match. Nothing changed.'}
Copy-Item -LiteralPath $backup -Destination $active -Force
if((Get-FileHash -LiteralPath $active).Hash -ne $expected){throw 'Restore verification failed'}
$prior=Join-Path $PSScriptRoot 'prior-v9-installation.json'
if(Test-Path -LiteralPath $prior){Copy-Item -LiteralPath $prior -Destination (Join-Path $usbRoot 'EFI\2503-HANDOFF\installation.json') -Force}
[ordered]@{RestoredUtc=[DateTime]::UtcNow.ToString('o');TestVersion=9;CanUnlock=$false;EfiSha256=$expected.ToLowerInvariant()} |
 ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'restored-v9.json') -Encoding UTF8
Write-Output 'Restored the hardware-tested version 9 Windows handoff image. Unlock is disabled.'
Write-Output 'No Windows driver, BIOS, VBIOS or boot order was changed.'
