function Test-AutoConfirmationEvidence {
 param([string]$LogText,[datetime]$LogUtc,[datetime]$BootUtc,[bool]$HasPending,[string]$PendingContent,
       [bool]$Disabled,[string]$ImageSha,[string]$ExpectedSha,[object[]]$Gpu,[int]$SmiExit,[string]$SmiLine,[string]$ExpectedDriver='581.80')
 $reason='ok'
 if(!$HasPending){$reason='no-pending'}
 elseif($Disabled){$reason='operator-disabled'}
 elseif($PendingContent -cne "2503-AUTO-PENDING-v11`r`n"){$reason='pending-content-mismatch'}
 elseif($ExpectedSha -notmatch '^[0-9a-fA-F]{64}$' -or $ImageSha -ine $ExpectedSha){$reason='auto-image-changed'}
 elseif(($BootUtc-$LogUtc).TotalSeconds -lt -10 -or ($BootUtc-$LogUtc).TotalSeconds -gt 45){$reason='not-current-boot'}
 elseif($Gpu.Count -ne 1 -or $Gpu[0].ConfigManagerErrorCode -ne 0 -or $Gpu[0].PNPDeviceID -notlike 'PCI\VEN_10DE&DEV_2503&SUBSYS_250310DE&REV_A1\*'){$reason='gpu-not-healthy-exact-target'}
 elseif($SmiExit -ne 0){$reason='nvml-not-ready'}
 else {
  $fields=@($SmiLine.Trim() -split ',' | ForEach-Object {$_.Trim()})
  if($fields.Count -ne 3 -or $fields[0] -ine '0x250310DE' -or $fields[1] -notmatch '^0+:07:00\.0$' -or $fields[2] -cne $ExpectedDriver){$reason='nvml-identity-or-driver-mismatch'}
  else {
   foreach($line in @('test-version=0x000000000000000b','automatic-pending-marker-written=0x0000000000000001',
    'unlock-core-return=0x0000000000000000','unlock-core-linux-result=0x0000000000000000',
    'unlock-core-complete=0x0000000000000001','vga-forwarding-restored-bme-still-off=0x0000000000000001',
    'preboot-check-result=0x0000000000000000','headless-ready=0x0000000000000001')){
     if(!$LogText.Contains($line)){$reason='incomplete-auto-log';break}
   }
   if($LogText.Contains('headless-windows-returned=') -or $LogText.Contains('manual-power-off-required=')){$reason='failed-windows-handoff'}
  }
 }
 [pscustomobject]@{Allowed=($reason -eq 'ok');Reason=$reason}
}
