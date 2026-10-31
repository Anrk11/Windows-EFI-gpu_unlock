$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'auto_confirm_policy.ps1')
function New-Case {
 @{LogText=(@('test-version=0x000000000000000b','automatic-pending-marker-written=0x0000000000000001',
 'unlock-core-return=0x0000000000000000','unlock-core-linux-result=0x0000000000000000',
 'unlock-core-complete=0x0000000000000001','vga-forwarding-restored-bme-still-off=0x0000000000000001',
 'preboot-check-result=0x0000000000000000','headless-ready=0x0000000000000001') -join "`r`n")
 LogUtc=[datetime]'2026-10-31T06:08:14Z';BootUtc=[datetime]'2026-10-31T06:08:17Z';HasPending=$true
 PendingContent="2503-AUTO-PENDING-v11`r`n";Disabled=$false;ImageSha=('a'*64);ExpectedSha=('a'*64)
 Gpu=@([pscustomobject]@{PNPDeviceID='PCI\VEN_10DE&DEV_2503&SUBSYS_250310DE&REV_A1\INSTANCE';ConfigManagerErrorCode=0})
 SmiExit=0;SmiLine='0x250310DE, 00000000:07:00.0, 581.80';ExpectedDriver='581.80'}
}
for($i=0;$i -lt 18;$i++){
 $data=New-Case
 switch($i){
  1 {$data.HasPending=$false} 2 {$data.PendingContent='wrong'} 3 {$data.Disabled=$true}
  4 {$data.LogUtc=$data.LogUtc.AddMinutes(-5)} 5 {$data.LogUtc=$data.LogUtc.AddMinutes(5)}
  6 {$data.ImageSha='b'*64} 7 {$data.LogText=$data.LogText.Replace('000000000000000b','000000000000000a')}
  8 {$data.LogText=$data.LogText.Replace('headless-ready=','missing=')}
  9 {$data.LogText+="`nheadless-windows-returned=0x0000000000000000"}
  10 {$data.Gpu[0].ConfigManagerErrorCode=12} 11 {$data.Gpu=@($data.Gpu[0],$data.Gpu[0])}
  12 {$data.SmiExit=6} 13 {$data.SmiLine='0x250310DE, 00000000:01:00.0, 581.80'}
  14 {$data.SmiLine='0x250310DE, 00000000:07:00.0, 616.80'}
  15 {$data.Gpu[0].PNPDeviceID='PCI\VEN_10DE&DEV_2504&SUBSYS_250310DE&REV_A1\INSTANCE'}
  16 {$data.LogText=''} 17 {$data.LogText+="`nmanual-power-off-required=0x0000000000000001"}
 }
 $r=Test-AutoConfirmationEvidence @data
 if($r.Allowed -ne ($i -eq 0)){throw "Policy case $i failed: $($r.Reason)"}
}
Write-Output 'PASS: 18 Windows auto-confirmation policy cases; no devices or files changed'
