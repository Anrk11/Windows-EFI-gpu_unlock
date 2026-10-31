param([string]$StateDirectory='C:\ProgramData\Codex2503AutoBoot')
$ErrorActionPreference='Stop'
if(!([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Administrator approval required to install an EFI startup entry'}
if([IO.Path]::GetFullPath($StateDirectory) -cne 'C:\ProgramData\Codex2503AutoBoot'){throw 'Unexpected protected state directory'}
$source=$PSScriptRoot;$build=Join-Path $source 'build-auto'
$repo=Split-Path (Split-Path $source -Parent) -Parent
$workspace=Split-Path $repo -Parent
. (Join-Path $source 'auto_bcd.ps1')
$mount=$null;$mounted=$false;$newId=$null;$oldOrder=@();$committed=$false;$probeName=$null;$registeredConfirmTask=$false;$taskName='Codex2503-EFI-BootConfirmation'
if(Test-Path -LiteralPath (Join-Path $StateDirectory 'installation.json')){throw 'An installation state already exists; inspect it before reinstalling'}
[void][IO.Directory]::CreateDirectory($StateDirectory)
& "$env:SystemRoot\System32\icacls.exe" $StateDirectory /inheritance:r /grant:r '*S-1-5-18:(OI)(CI)F' '*S-1-5-32-544:(OI)(CI)F' '*S-1-5-32-545:(OI)(CI)RX' | Out-Null
if($LASTEXITCODE -ne 0){throw 'Could not protect the startup payload directory'}
function Progress([string]$Phase){[ordered]@{Utc=[DateTime]::UtcNow.ToString('o');Phase=$Phase} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $StateDirectory 'install-progress.json') -Encoding UTF8}
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Stage([string]$From,[string]$To,[string]$Expected){
 if((Hash $From) -cne $Expected.ToLowerInvariant()){throw ('Source hash mismatch: '+$From)}
 [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($To))
 if(Test-Path -LiteralPath $To){if((Hash $To) -cne $Expected.ToLowerInvariant()){throw ('Different existing file: '+$To)}}else{[IO.File]::Copy($From,$To,$false)}
 if((Hash $To) -cne $Expected.ToLowerInvariant()){throw ('Readback mismatch: '+$To)}
}
try {
 Progress 'preflight'
 $manifest=Get-Content -LiteralPath (Join-Path $build 'manifest.json') -Raw | ConvertFrom-Json
 if($manifest.TestVersion -ne 11 -or !$manifest.Automatic -or $manifest.ScenariosPassed -ne 47 -or $manifest.CoreObjectParityCount -ne 14 -or !$manifest.PriorUsbVersion10HardwareVerified){throw 'Automatic build/test gates incomplete'}
 foreach($s in $manifest.Sources.PSObject.Properties){if((Hash (Join-Path $repo $s.Name)) -cne $s.Value){throw ('Build source changed: '+$s.Name)}}
 if((Hash (Join-Path $build 'AUTO.EFI')) -cne $manifest.EfiSha256){throw 'Automatic image changed'}
 $partition=Get-Partition -DiskNumber 2 -PartitionNumber 1
 if(!$partition.IsSystem -or $partition.Guid -ine '{9d69eaa3-1dea-4abf-8c4b-236d216b0d80}' -or $partition.GptType -ine '{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}'){throw 'System ESP identity changed'}
 $secure=$null
 try{$secure=Confirm-SecureBootUEFI}catch{
  $record=Get-ItemProperty -LiteralPath 'HKLM:\SYSTEM\CurrentControlSet\Control\SecureBoot\State' -Name UEFISecureBootEnabled -ErrorAction Stop
  if($record.UEFISecureBootEnabled -eq 0){$secure=$false}else{throw 'Cannot confirm Secure Boot is disabled'}
 }
 if($secure){throw 'Unsigned custom EFI requires Secure Boot disabled; installer does not change firmware settings'}
 $enc=Get-CimInstance -Namespace 'root\CIMV2\Security\MicrosoftVolumeEncryption' -ClassName Win32_EncryptableVolume -Filter "DriveLetter='C:'"
 if(!$enc){throw 'Cannot determine C: encryption state'}
 $conversion=Invoke-CimMethod -InputObject $enc -MethodName GetConversionStatus
 $protection=Invoke-CimMethod -InputObject $enc -MethodName GetProtectionStatus
 if($conversion.ReturnValue -ne 0 -or $protection.ReturnValue -ne 0 -or $conversion.ConversionStatus -ne 0 -or $protection.ProtectionStatus -ne 0){throw 'C: must be fully decrypted and protection off; encryption settings were not changed'}
 $powerKey='HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Power'
 $hiber=Get-ItemProperty -LiteralPath $powerKey -Name HiberbootEnabled -ErrorAction SilentlyContinue
 if($null -ne $hiber -and $hiber.HiberbootEnabled -ne 0){throw 'Fast Startup must already be disabled; no power settings were changed'}
 foreach($letter in @('S','T','U','V','W','Y')){if(!(Get-PSDrive -Name $letter -ErrorAction SilentlyContinue)){$mount=$letter;break}}
 if(!$mount){throw 'No unused temporary ESP drive letter'}
 & "$env:SystemRoot\System32\mountvol.exe" ($mount+':') /S
 if($LASTEXITCODE -ne 0){throw 'Could not temporarily mount the existing ESP'}
 $mounted=$true
 $volumeRoot='\\?\Volume{9d69eaa3-1dea-4abf-8c4b-236d216b0d80}\'
 $volumeName=@(& "$env:SystemRoot\System32\mountvol.exe" ($mount+':') /L) -join "`n"
 if($LASTEXITCODE -ne 0 -or $volumeName.IndexOf($volumeRoot,[StringComparison]::OrdinalIgnoreCase) -lt 0){throw 'Mounted ESP volume identity mismatch'}
 $volume=@(Get-CimInstance Win32_Volume -Filter "FileSystem='FAT32'" | Where-Object {$_.DeviceID -ieq $volumeRoot})
 if($volume.Count -ne 1 -or !$volume[0].SystemVolume){throw 'System FAT32 volume identity mismatch'}
 $esp=$mount+':\';$folder=Join-Path $esp 'EFI\Codex2503'
 $required=[long](($manifest.Assets | Measure-Object -Property Bytes -Sum).Sum)+[long](Get-Item -LiteralPath (Join-Path $build 'AUTO.EFI')).Length+64MB
 if($volume[0].FreeSpace -lt $required){throw 'ESP lacks required space plus 64 MiB reserve'}
 foreach($name in @('PENDING.TXT','LASTRUN.TXT','DISABLED.TXT')){if(Test-Path -LiteralPath (Join-Path $folder $name)){throw ('Existing automatic state requires review: '+$name)}}
 Progress 'staging-verified-files'
 Stage (Join-Path $build 'AUTO.EFI') (Join-Path $folder 'AUTO.EFI') $manifest.EfiSha256
 foreach($a in $manifest.Assets){$asset=Join-Path (Join-Path $workspace 'rtx3060-unlock-assets\assets') $a.Name;if((Get-Item -LiteralPath $asset).Length -ne $a.Bytes){throw 'Asset length changed'};Stage $asset (Join-Path (Join-Path $folder 'ASSETS') $a.Name) $a.Sha256}
 Stage (Join-Path $build 'manifest.json') (Join-Path $folder 'manifest-v11.json') (Hash (Join-Path $build 'manifest.json'))
 foreach($name in @('auto_confirm.ps1','auto_confirm_policy.ps1','auto_bcd.ps1','auto_restore.ps1')){Stage (Join-Path $source $name) (Join-Path $StateDirectory $name) (Hash (Join-Path $source $name))}
 $gpu=@(Get-CimInstance Win32_PnPEntity -Filter "PNPDeviceID LIKE 'PCI%VEN_10DE%DEV_2503%'" | Where-Object {$_.ConfigManagerErrorCode -eq 0})
 if($gpu.Count -ne 1){throw 'Expected one healthy captured NVIDIA GPU'}
 $smi=(Get-Command nvidia-smi.exe -ErrorAction Stop).Source
 $arm=[ordered]@{EspRoot='\\?\Volume{9d69eaa3-1dea-4abf-8c4b-236d216b0d80}\EFI\Codex2503\';EfiSha256=$manifest.EfiSha256;ExpectedPnpId=$gpu[0].PNPDeviceID;ExpectedDriver='581.80';NvidiaSmi=$smi}
 $arm | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $StateDirectory 'arm.json') -Encoding UTF8
 $ps="$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
 $arguments='-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "'+(Join-Path $StateDirectory 'auto_confirm.ps1')+'" -StateDirectory "'+$StateDirectory+'"'
 $principal=New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount -RunLevel Highest
 $settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 5) -MultipleInstances IgnoreNew -StartWhenAvailable
 Progress 'checking-real-background-token-and-ESP-access'
 $probeName='Codex2503-AutoConfirm-Probe-'+[guid]::NewGuid().ToString('N')
 $probeAction=New-ScheduledTaskAction -Execute $ps -Argument ($arguments+' -Probe')
 Register-ScheduledTask -TaskName $probeName -Action $probeAction -Principal $principal -Settings $settings -Force | Out-Null
 Start-ScheduledTask -TaskName $probeName
 $probe=$null
 for($wait=0;$wait -lt 90;$wait++){
  $file=Join-Path $StateDirectory 'probe-result.json'
  if(Test-Path -LiteralPath $file){try{$probe=Get-Content -LiteralPath $file -Raw | ConvertFrom-Json}catch{};if($probe -and (Get-ScheduledTask -TaskName $probeName).State -ne 'Running'){break}}
  Start-Sleep -Seconds 1
 }
 if(!$probe -or !$probe.Ready -or !$probe.AdministratorToken -or !$probe.EspReadable -or $probe.DeviceModified -or $probe.Confirmed -or (Get-ScheduledTaskInfo -TaskName $probeName).LastTaskResult -ne 0){throw 'Actual background probe failed; boot order unchanged'}
 Unregister-ScheduledTask -TaskName $probeName -Confirm:$false;$probeName=$null
 Progress 'backing-up-existing-boot-configuration'
 $fw=Invoke-AutoBcd @('/enum','{fwbootmgr}','/v');$oldOrder=@(Get-AutoFirmwareOrder $fw)
 if($fw -match '(?mi)^(?:bootsequence|\u542f\u52a8\u987a\u5e8f)\s+'){throw 'A one-time firmware boot is scheduled; refusing to replace it'}
 $manager=Invoke-AutoBcd @('/enum','{bootmgr}','/v');$windowsId=Get-AutoBcdId $manager
 if($oldOrder -notcontains $windowsId -or $manager -notmatch '(?i)\\EFI\\Microsoft\\Boot\\bootmgfw\.efi'){throw 'Original Windows Boot Manager mismatch'}
 $backup=Join-Path (Join-Path $StateDirectory 'backups') (Get-Date -Format 'yyyyMMdd-HHmmss')
 [void][IO.Directory]::CreateDirectory($backup)
 [void](Invoke-AutoBcd @('/export',(Join-Path $backup 'BCD-export')))
 [IO.File]::WriteAllText((Join-Path $backup 'firmware-before.txt'),$fw)
 [IO.File]::WriteAllText((Join-Path $backup 'windows-bootmgr-before.txt'),$manager)
 $windowsFile=Join-Path $esp 'EFI\Microsoft\Boot\bootmgfw.efi';$windowsHash=Hash $windowsFile
 [IO.File]::Copy($windowsFile,(Join-Path $backup 'bootmgfw.efi'),$false)
 $state=[ordered]@{Mode='registering';InstalledUtc=[DateTime]::UtcNow.ToString('o');EntryId=$null;WindowsId=$windowsId;BootOrderBefore=$oldOrder;Backup=$backup;EfiSha256=$manifest.EfiSha256;WindowsBootManagerSha256=$windowsHash;HardwareAutoBootTested=$false;UserTrainingConfirmed=$true;BackgroundProbeVerified=$true;EspGuid=$partition.Guid;FastStartupAlreadyDisabled=$true}
 $state | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $StateDirectory 'installation.json') -Encoding UTF8
 if(Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue){throw 'Startup confirmation task already exists'}
 $trigger=New-ScheduledTaskTrigger -AtStartup;$trigger.Delay='PT15S'
 $action=New-ScheduledTaskAction -Execute $ps -Argument $arguments
 Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'Archive and confirm a healthy current GPU unlock boot; never resets devices or runs compute.' | Out-Null
 $registeredConfirmTask=$true
 if((Hash $windowsFile) -cne $windowsHash -or (Hash (Join-Path $folder 'AUTO.EFI')) -cne $manifest.EfiSha256 -or (Invoke-AutoBcd @('/enum','{fwbootmgr}','/v')) -cne $fw){throw 'Boot state changed before registration'}
 Progress 'registering-separate-EFI-entry'
 $copy=Invoke-AutoBcd @('/copy','{bootmgr}','/d','RTX 3060 Compute Unlock')
 $ids=@([regex]::Matches($copy,'\{[0-9a-fA-F-]{36}\}') | ForEach-Object {$_.Value.ToLowerInvariant()})
 if($ids.Count -ne 1){throw 'Could not uniquely identify the new boot entry'}
 $newId=$ids[0];$state.EntryId=$newId
 $state | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $StateDirectory 'installation.json') -Encoding UTF8
 [void](Invoke-AutoBcd @('/set',$newId,'device',('partition='+$mount+':')))
 [void](Invoke-AutoBcd @('/set',$newId,'path','\EFI\Codex2503\AUTO.EFI'))
 $entry=Invoke-AutoBcd @('/enum',$newId,'/v')
 if($entry -notmatch '(?i)\\EFI\\Codex2503\\AUTO\.EFI' -or $entry -notmatch 'RTX 3060 Compute Unlock'){throw 'New EFI entry readback failed'}
 $newOrder=@($newId,$windowsId)+@($oldOrder | Where-Object {$_ -ine $windowsId -and $_ -ine $newId})
 Set-AutoFirmwareOrder $newOrder
 if((Hash $windowsFile) -cne $windowsHash -or (Invoke-AutoBcd @('/enum','{bootmgr}','/v')) -cne $manager){throw 'Original Windows Boot Manager changed'}
 $state.Mode='automatic';$state.BootOrderAfter=$newOrder;$state.TaskName=$taskName
 $state | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $StateDirectory 'installation.json') -Encoding UTF8
 $desktop=[Environment]::GetFolderPath('Desktop')
 $cmd='@echo off'+"`r`n"+'"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "C:\ProgramData\Codex2503AutoBoot\auto_restore.ps1"'+"`r`npause`r`n"
 [IO.File]::WriteAllText((Join-Path $desktop 'DISABLE-GPU-AUTO.cmd'),$cmd,[Text.Encoding]::ASCII)
 $committed=$true
 Progress 'complete-no-reboot-performed'
 $state | ConvertTo-Json -Depth 6
}catch{
 $failure=$_
 if($oldOrder.Count){try{Set-AutoFirmwareOrder $oldOrder}catch{Write-Warning ('Order rollback failed: '+$_.Exception.Message)}}
 if($newId){try{[void](Invoke-AutoBcd @('/delete',$newId))}catch{Write-Warning 'Unreferenced custom entry may remain'}}
 if($registeredConfirmTask){Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue}
 Progress ('failed: '+$failure.Exception.Message)
 $failure | Out-String | Set-Content -LiteralPath (Join-Path $StateDirectory 'install-error.txt') -Encoding UTF8
 throw $failure
}finally{
 if($probeName){Unregister-ScheduledTask -TaskName $probeName -Confirm:$false -ErrorAction SilentlyContinue}
 if($mounted){& "$env:SystemRoot\System32\mountvol.exe" ($mount+':') /D | Out-Null}
}
