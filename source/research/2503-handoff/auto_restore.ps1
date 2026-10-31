param([string]$StateDirectory=$PSScriptRoot)
$ErrorActionPreference='Stop'
if(!([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){
 $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -StateDirectory "'+$StateDirectory+'"'
 $child=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -WindowStyle Hidden -ArgumentList $arguments -PassThru
 $null=$child.Handle;$child.WaitForExit();exit $child.ExitCode
}
. (Join-Path $PSScriptRoot 'auto_bcd.ps1')
$state=Get-Content -LiteralPath (Join-Path $StateDirectory 'installation.json') -Raw | ConvertFrom-Json
$current=@(Get-AutoFirmwareOrder (Invoke-AutoBcd @('/enum','{fwbootmgr}','/v')))
if($state.WindowsId -notmatch '^\{[0-9a-f-]{36}\}$' -or $current -notcontains $state.WindowsId){throw 'Saved Windows Boot Manager is missing'}
$first=@($state.WindowsId)+@($current | Where-Object {$_ -ine $state.WindowsId})
Set-AutoFirmwareOrder $first
# Order recovery works even if the ESP cannot be mounted or the files changed.
try{[IO.File]::WriteAllText('\\?\Volume{9d69eaa3-1dea-4abf-8c4b-236d216b0d80}\EFI\Codex2503\DISABLED.TXT',"Operator disabled automatic GPU unlock.`r`n")}catch{Write-Warning 'Windows is first; could not also create the disabled marker'}
$state | Add-Member -NotePropertyName Mode -NotePropertyValue 'windows-first-disabled' -Force
$state | Add-Member -NotePropertyName RestoredUtc -NotePropertyValue ([DateTime]::UtcNow.ToString('o')) -Force
$state | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $StateDirectory 'installation.json') -Encoding UTF8
Write-Output 'Windows Boot Manager is first. Automatic GPU unlock disabled. No files deleted and no reboot.'
