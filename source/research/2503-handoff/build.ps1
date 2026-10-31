param([string]$Clang='D:\codex_work\autodl\probe-toolchain\clang-wsl.cmd',
      [string]$LldLink='D:\codex_work\autodl\probe-toolchain\lld-link-wsl.cmd',
      [string]$Python='python', [switch]$HeadlessWindowsTest,[switch]$RestoreVgaDecode)
$ErrorActionPreference='Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
 New-Item -ItemType Directory -Path build -Force | Out-Null
 $sources=@('main.c','transaction.c','dma_probe.c','../../internal-boot/efi_runtime.c','../../internal-boot/unlock/sha256.c')
 $objects=@()
 $extra=@();if($HeadlessWindowsTest){$extra+='-DHT_HEADLESS_WINDOWS_TEST'}
 if($RestoreVgaDecode){if(!$HeadlessWindowsTest){throw 'VGA restore requires the headless Windows test'};$extra+='-DHT_RESTORE_VGA_DECODE'}
 foreach($source in $sources){
  $obj='build/'+[IO.Path]::GetFileNameWithoutExtension($source)+'.obj'
  & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror @extra -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c $source -o $obj
  if($LASTEXITCODE -ne 0){throw "Compile failed: $source"}
  $objects+=$obj
 }
 & $LldLink /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /map:build/HANDOFF.map /out:build/HANDOFF.EFI @objects
 if($LASTEXITCODE -ne 0){throw 'Link failed'}
 & $Python -I ../../internal-boot/unlock/verify_pe.py build/HANDOFF.EFI build/HANDOFF.map --output build/pe-verification.json
 if($LASTEXITCODE -ne 0){throw 'PE verification failed'}
 $sourceHashes=@($sources+@('transaction.h','windows.h','dma_probe.h','../../internal-boot/boot/media.h','../../internal-boot/boot/platform-config.h') | ForEach-Object {[ordered]@{Path=$_;Sha256=(Get-FileHash -LiteralPath $_).Hash.ToLowerInvariant()}})
 $mode=if($HeadlessWindowsTest){'2503-batch-then-windows-without-gop-reconnect'}else{'2503-batch-handoff-decode-layout-prom-dma-map-unmap-then-windows'}
 if($RestoreVgaDecode){$mode='2503-preserve-entry-vga-decode-then-windows'}
 [ordered]@{Mode=$mode;CanDetach=$true;CanUnlock=$false;HardwareTested=$false
  RequiresManualT=$true;StartsWindows=$true;EnablesIoMemOnly=(!$RestoreVgaDecode);EnablesBusMaster=$false;ResetsGpu=$false;WritesFirmwareVariables=$false
  MapsDmaBuffers=$true;SubmitsGpuWork=$false;ExecutesFirmware=$false
  HeadlessWindowsTest=[bool]$HeadlessWindowsTest
  TestVersion=$(if($RestoreVgaDecode){9}elseif($HeadlessWindowsTest){8}else{7});RestoresDisplayBeforeWindows=(!$HeadlessWindowsTest)
  RestoresEntryVgaDecode=[bool]$RestoreVgaDecode
  EfiSha256=(Get-FileHash -LiteralPath build/HANDOFF.EFI).Hash.ToLowerInvariant();Sources=$sourceHashes
 } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath build/manifest.json -Encoding utf8
 Write-Output 'Built build/HANDOFF.EFI. Experimental; no hardware test performed. USB boot files not changed.'
} finally {Pop-Location}
