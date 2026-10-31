$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'auto_bcd.ps1')
$a='{9dea862c-5cdd-4e70-acc1-f32b344d4795}';$b='{11111111-2222-3333-4444-555555555555}'
$zh=([regex]::Unescape('\u663e\u793a\u987a\u5e8f'))
foreach($label in @('displayorder',$zh)){
 $text="identifier              {a5a30fa2-3d06-4e9f-b5f4-a01df9d1fcba}`r`n$label            $a`r`n                        $b`r`ntimeout                 0`r`n"
 $ids=@(Get-AutoFirmwareOrder $text)
 if(($ids -join '|') -cne ($a+'|'+$b)){throw 'Localized firmware-order parse failed'}
}
foreach($bad in @('none',"displayorder $a`n             $a")){
 $failed=$false;try{$null=Get-AutoFirmwareOrder $bad}catch{$failed=$true};if(!$failed){throw 'Bad order was accepted'}
}
if((Get-AutoBcdId "object $a") -cne $a){throw 'Full identifier parse failed'}
Write-Output 'PASS: 5 localized BCD parsing cases; no BCD commands executed'
