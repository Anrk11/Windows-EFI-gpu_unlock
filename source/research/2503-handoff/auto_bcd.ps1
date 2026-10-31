function Invoke-AutoBcd([string[]]$Arguments){
 $lines=@(& "$env:SystemRoot\System32\bcdedit.exe" @Arguments)
 if($LASTEXITCODE -ne 0){throw ('BCDEdit failed: '+($Arguments -join ' '))}
 $lines -join "`r`n"
}
function Get-AutoBcdId([string]$Text){
 $m=[regex]::Match($Text,'\{[0-9a-fA-F-]{36}\}')
 if(!$m.Success){throw 'BCD identifier missing'}
 $m.Value.ToLowerInvariant()
}
function Get-AutoFirmwareOrder([string]$Text){
 $m=[regex]::Match($Text,'(?ms)^(?:displayorder|\u663e\u793a\u987a\u5e8f)\s+(.*?)(?=\r?\n\S|\z)')
 if(!$m.Success){throw 'Firmware display order missing'}
 $ids=@([regex]::Matches($m.Groups[1].Value,'\{(?:bootmgr|[0-9a-fA-F-]{36})\}') | ForEach-Object {$_.Value.ToLowerInvariant()})
 if(!$ids.Count -or @($ids | Select-Object -Unique).Count -ne $ids.Count){throw 'Invalid firmware display order'}
 $ids
}
function Set-AutoFirmwareOrder([string[]]$Ids){
 if(!$Ids.Count){throw 'Refusing empty firmware order'}
 foreach($id in $Ids){if($id -notmatch '^\{(?:bootmgr|[0-9a-fA-F-]{36})\}$'){throw 'Invalid firmware identifier'}}
 [void](Invoke-AutoBcd (@('/set','{fwbootmgr}','displayorder')+$Ids))
 $actual=@(Get-AutoFirmwareOrder (Invoke-AutoBcd @('/enum','{fwbootmgr}','/v')))
 if(($actual -join '|') -ine ($Ids -join '|')){throw 'Firmware boot-order readback mismatch'}
}
