#!/bin/sh
# CPU/RAM-only preparation and allocator tests; never accesses a GPU.
set -eu
cd "$(dirname "$0")"
upstream=$1
work=$2
prom=$3
capture=$4
gcc -std=c11 -Wall -Wextra -Werror -I "$upstream/kernel/gpu_unlock" \
  offline_transaction.c "$upstream/kernel/gpu_unlock/prep_610.c" \
  "$upstream/kernel/gpu_unlock/layout_610.c" "$upstream/kernel/gpu_unlock/vbios_610.c" \
  -o "$work/offline-transaction"
"$work/offline-transaction" "$work/assets" "$prom" "$capture" \
  > "$work/offline-preparation.json" 2> "$work/offline-preparation.log"
gcc -std=c11 -D_POSIX_C_SOURCE=200112L -DGU_HOST_WRITE_TEST -fshort-wchar \
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wno-misleading-indentation \
  -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  test_dma_port.c ../../internal-boot/unlock/port.c -Wl,--gc-sections -o "$work/test-dma-port"
"$work/test-dma-port" > "$work/dma-port-tests.txt"
cat "$work/offline-preparation.json" "$work/dma-port-tests.txt"
