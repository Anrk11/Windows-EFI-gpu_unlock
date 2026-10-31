param([string]$StateDirectory='C:\ProgramData\Codex2503AutoBoot',[switch]$Probe)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'auto_confirm_policy.ps1')
$result=[ordered]@{StartedUtc=[DateTime]::UtcNow.ToString('o');Probe=[bool]$Probe;Ready=$false;Confirmed=$false;DeviceModified=$false;PerformanceMeasured=$false;Reason='starting'}
$destination=Join-Path $StateDirectory $(if($Probe){'probe-result.json'}else{'last-confirmation.json'})
function Hash-Bytes([byte[]]$bytes){$sha=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}}
function Query-Nvidia([string]$Executable){
 $p=New-Object Diagnostics.Process
 $p.StartInfo.FileName=$Executable;$p.StartInfo.Arguments='--query-gpu=pci.device_id,pci.bus_id,driver_version --format=csv,noheader'
 $p.StartInfo.UseShellExecute=$false;$p.StartInfo.CreateNoWindow=$true;$p.StartInfo.RedirectStandardOutput=$true;$p.StartInfo.RedirectStandardError=$true
 try{
  if(!$p.Start()){throw 'nvidia-smi did not start'};$null=$p.Handle
  if(!$p.WaitForExit(15000)){try{$p.Kill()}catch{};throw 'nvidia-smi timed out'}
  [pscustomobject]@{ExitCode=$p.ExitCode;Output=$p.StandardOutput.ReadToEnd().Trim();Error=$p.StandardError.ReadToEnd()}
 }finally{$p.Dispose()}
}
try {
 $arm=Get-Content -LiteralPath (Join-Path $StateDirectory 'arm.json') -Raw | ConvertFrom-Json
 if($arm.EspRoot -cne '\\?\Volume{9d69eaa3-1dea-4abf-8c4b-236d216b0d80}\EFI\Codex2503\'){throw 'Unexpected ESP root'}
 $admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
 $result.AdministratorToken=$admin;if(!$admin){throw 'Administrator token required'}
 $image=$arm.EspRoot+'AUTO.EFI';$pending=$arm.EspRoot+'PENDING.TXT';$log=$arm.EspRoot+'LASTRUN.TXT';$disabled=$arm.EspRoot+'DISABLED.TXT'
 $imageHash=Hash-Bytes ([IO.File]::ReadAllBytes($image));$result.ImageSha256=$imageHash
 if($imageHash -ine $arm.EfiSha256){throw 'Installed EFI changed'}
 $result.EspReadable=$true
 $boot=(Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToUniversalTime();$result.BootUtc=$boot.ToString('o')
 $gpu=@();$smi=$null
 for($try=0;$try -lt 12;$try++){
  $gpu=@(Get-CimInstance Win32_PnPEntity -Filter "PNPDeviceID LIKE 'PCI%VEN_10DE%DEV_2503%'" | Where-Object {$_.PNPDeviceID -ieq $arm.ExpectedPnpId} | Select-Object PNPDeviceID,ConfigManagerErrorCode)
  $smi=Query-Nvidia $arm.NvidiaSmi
  if($gpu.Count -eq 1 -and $gpu[0].ConfigManagerErrorCode -eq 0 -and $smi.ExitCode -eq 0){break}
  Start-Sleep -Seconds 5
 }
 $result.Gpu=$gpu;$result.NvidiaSmi=$smi
 $fields=@($smi.Output -split ',' | ForEach-Object {$_.Trim()})
 $result.Ready=($gpu.Count -eq 1 -and $gpu[0].ConfigManagerErrorCode -eq 0 -and $smi.ExitCode -eq 0 -and
  $fields.Count -eq 3 -and $fields[0] -ieq '0x250310DE' -and $fields[1] -match '^0+:07:00\.0$' -and $fields[2] -ceq $arm.ExpectedDriver)
 if($Probe){$result.Reason=if($result.Ready){'background-token-ESP-and-driver-probe-passed'}else{'driver-probe-failed'}}
 else {
  $hasPending=[IO.File]::Exists($pending)
  $content=if($hasPending){[IO.File]::ReadAllText($pending)}else{''}
  [byte[]]$bytes=@();if([IO.File]::Exists($log)){$bytes=[IO.File]::ReadAllBytes($log)}
  $logUtc=if([IO.File]::Exists($log)){[IO.File]::GetLastWriteTimeUtc($log)}else{[datetime]::MinValue}
  $evidence=@{LogText=[Text.Encoding]::ASCII.GetString($bytes);LogUtc=$logUtc;BootUtc=$boot;
   HasPending=$hasPending;PendingContent=$content;Disabled=[IO.File]::Exists($disabled);ImageSha=$imageHash;ExpectedSha=$arm.EfiSha256;
   Gpu=$gpu;SmiExit=$smi.ExitCode;SmiLine=$smi.Output;ExpectedDriver=$arm.ExpectedDriver}
  $decision=Test-AutoConfirmationEvidence @evidence
  $result.Reason=$decision.Reason
  if($decision.Allowed){
   $history=Join-Path (Join-Path $StateDirectory 'history') ($boot.ToString('yyyyMMdd-HHmmss')+'-'+$PID)
   [void][IO.Directory]::CreateDirectory($history)
   $copy=Join-Path $history 'LASTRUN.TXT';[IO.File]::WriteAllBytes($copy,$bytes)
   $hash=Hash-Bytes $bytes
   if((Hash-Bytes ([IO.File]::ReadAllBytes($copy))) -cne $hash -or (Hash-Bytes ([IO.File]::ReadAllBytes($log))) -cne $hash){throw 'Log changed while archiving'}
   if([IO.File]::ReadAllText($pending) -cne $content -or [IO.File]::Exists($disabled) -or (Hash-Bytes ([IO.File]::ReadAllBytes($image))) -ine $arm.EfiSha256){throw 'Confirmation state changed'}
   $result.LogArchive=$copy;$result.LogSha256=$hash
   # Only these two fixed files inside the pinned ESP directory are removed.
   [IO.File]::Delete($log)
   [IO.File]::Delete($pending)
   $result.Confirmed=$true;$result.Reason='current-boot-driver-healthy-log-archived-next-boot-armed'
  }
 }
}catch{$result.Reason=$_.Exception.Message;$result.Error=$_.ToString()}
finally{
 $result.FinishedUtc=[DateTime]::UtcNow.ToString('o')
 [IO.File]::WriteAllText($destination,($result | ConvertTo-Json -Depth 7),(New-Object Text.UTF8Encoding($false)))
}
if($Probe -and !$result.Ready){exit 1}
if(!$Probe -and !$result.Confirmed -and $result.Reason -notin @('no-pending','operator-disabled')){exit 2}
