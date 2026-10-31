#!/bin/sh
set -eu
cd "$(dirname "$0")"
out=build-unlock
mkdir -p "$out"
core=../../internal-boot/unlock
python3 unlock_manifest.py snapshot "$out/build-inputs-before.json"
toolchain=${GU_2503_TOOLCHAIN_ROOT:-/mnt/d/codex_work/autodl/probe-toolchain/wsl-llvm/root}
export LD_LIBRARY_PATH="$toolchain/usr/lib/x86_64-linux-gnu:$toolchain/usr/lib/llvm-21/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
objects=""
for source in main.c transaction.c dma_probe.c core_bridge.c "$core/runtime.c" "$core/sha256.c" "$core/format.c" "$core/checks.c" "$core/port.c" "$core/compat.c" "$core/vendor/unlock_610.c" "$core/vendor/prep_610.c" "$core/vendor/layout_610.c" "$core/vendor/vbios_610.c" "$core/vendor/gfw_eval.c"; do
  base=$(basename "$source" .c)
  "$toolchain/usr/lib/llvm-21/bin/clang" -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector \
    -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter \
    -D__KERNEL__ -DGU_UEFI_PORT -DGU_EXPERIMENTAL_3090 -DGU_ENTRY_LATCHED_SS1_FULL \
    -DGU_RESEARCH_2503_FIRMWARE -DHT_EXECUTE_CORE -DHT_HEADLESS_WINDOWS_TEST -DHT_RESTORE_VGA_DECODE \
    -I "$core/compat" -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
    -c "$source" -o "$out/$base.obj"
  objects="$objects $out/$base.obj"
done
"$toolchain/usr/lib/llvm-21/bin/lld-link" /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 \
  /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /map:"$out/UNLOCK.map" /out:"$out/UNLOCK.EFI" $objects
python3 "$core/verify_pe.py" "$out/UNLOCK.EFI" "$out/UNLOCK.map" --output "$out/pe-verification.json"
python3 unlock_manifest.py finish
