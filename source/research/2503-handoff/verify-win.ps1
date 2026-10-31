$ErrorActionPreference='Stop'
$usbRoot=(Split-Path -Qualifier $PSScriptRoot)+'\'
$log=Join-Path $usbRoot '2503-UNLOCK.TXT'
$text=if(Test-Path -LiteralPath $log){Get-Content -LiteralPath $log -Raw}else{''}
$boot=(Get-CimInstance Win32_OperatingSystem).LastBootUpTime
$logTime=if(Test-Path -LiteralPath $log){(Get-Item -LiteralPath $log).LastWriteTimeUtc}else{$null}
$fresh=($null -ne $logTime -and [Math]::Abs(($logTime-$boot.ToUniversalTime()).TotalSeconds) -le 30)
$installed=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'installation.json') -Raw | ConvertFrom-Json
$activeHash=(Get-FileHash -LiteralPath (Join-Path $usbRoot 'EFI\BOOT\BOOTX64.EFI')).Hash.ToLowerInvariant()
$matchesImage=($activeHash -ceq $installed.EfiSha256)
$coreComplete=($text.Contains('test-version=0x000000000000000a') -and $text.Contains('unlock-core-complete=0x0000000000000001'))
$gpu=@(Get-CimInstance Win32_PnPEntity -Filter "PNPDeviceID LIKE 'PCI%VEN_10DE%DEV_2503%'" | Select-Object Name,PNPDeviceID,ConfigManagerErrorCode)
$smi=& nvidia-smi --query-gpu=name,pci.device_id,pci.bus_id,driver_version --format=csv,noheader
$smiExit=$LASTEXITCODE
$result=[ordered]@{
 CheckedUtc=[DateTime]::UtcNow.ToString('o');LastBootUpTime=$boot.ToString('o')
 LogPresent=(Test-Path -LiteralPath $log)
 CoreCompletedInLog=$coreComplete;LogLastWriteUtc=$logTime
 LogTimestampNearThisBoot=$fresh;ActiveEfiMatchesUnlockImage=$matchesImage
 CoreCompletedThisBoot=($coreComplete -and $fresh -and $matchesImage)
 WindowsStartPrepared=$text.Contains('headless-ready=0x0000000000000001')
 WindowsBootReturned=$text.Contains('headless-windows-returned=')
 DriverHealthy=($gpu.Count -eq 1 -and $gpu[0].ConfigManagerErrorCode -eq 0 -and $smiExit -eq 0)
 PresentGpu=$gpu;NvidiaSmi=$smi;NvidiaSmiExitCode=$smiExit
 WindowsVisibleVerified=$false;PerformanceVerified=$false
}
$dest=Join-Path $PSScriptRoot ('windows-check-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.json')
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $dest -Encoding UTF8
$result | ConvertTo-Json -Depth 5
Write-Output "Saved $dest. Driver health alone does not prove a performance increase."
