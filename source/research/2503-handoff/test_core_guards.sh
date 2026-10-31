#!/bin/sh
set -eu
cd "$(dirname "$0")"
out=$1
core=../../internal-boot/unlock
gcc -std=c11 -D_POSIX_C_SOURCE=200112L -DGU_HOST_WRITE_TEST -fshort-wchar \
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wno-misleading-indentation \
  -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  test_dma_port.c "$core/port.c" -Wl,--gc-sections -o "$out/test-default-port"
"$out/test-default-port" > "$out/default-port.txt"
cat "$out/default-port.txt"
gcc -std=c11 -D_POSIX_C_SOURCE=200112L -D__KERNEL__ -DGU_UEFI_PORT -DGU_EXPERIMENTAL_3090 \
  -DGU_PROFILE_H610M_3060 -DGU_ENTRY_LATCHED_SS1_FULL -fshort-wchar \
  -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -Wno-misleading-indentation \
  -I "$core/compat" -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  -c "$core/tests/test_efi_transaction.c" -o "$out/reviewed-test-compile.o"
if gcc -std=c11 -fshort-wchar -ffreestanding -DGU_RESEARCH_2503_HOST \
  -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  -c "$core/port.c" -o "$out/forbidden.o" 2> "$out/firmware-build-rejected.txt"; then
  echo 'ERROR: host-only model unexpectedly compiled as firmware' >&2
  exit 1
fi
python3 -c 'import sys; assert "2503 core model is forbidden in firmware builds" in open(sys.argv[1]).read()' "$out/firmware-build-rejected.txt"
echo 'PASS: unchanged reviewed fixture compiles; host-only model rejected by firmware compilation'
