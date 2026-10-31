#!/bin/sh
set -eu
cd "$(dirname "$0")"
capture=$1
work=$2
out=build-unlock
mkdir -p "$out"
core=../../internal-boot/unlock
python3 unlock_manifest.py snapshot "$out/frontend-inputs-before.json"
gcc -std=c11 -O1 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L -D__KERNEL__ -DGU_UEFI_PORT \
  -DGU_RESEARCH_2503_HOST -DGU_EXPERIMENTAL_3090 -DGU_ENTRY_LATCHED_SS1_FULL \
  -DGU_2503_ADAPTER_HOST_TEST -DHT_EXECUTE_CORE -DHT_HEADLESS_WINDOWS_TEST -DHT_RESTORE_VGA_DECODE \
  -fshort-wchar -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -Wno-misleading-indentation \
  -I "$core/compat" -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  test_core_frontend.c core_bridge.c transaction.c dma_probe.c "$core/sha256.c" "$core/format.c" \
  "$core/checks.c" "$core/port.c" "$core/compat.c" "$core/vendor/unlock_610.c" \
  "$core/vendor/prep_610.c" "$core/vendor/layout_610.c" "$core/vendor/vbios_610.c" "$core/vendor/gfw_eval.c" \
  -o "$out/test-core-frontend"
"$out/test-core-frontend" "$capture/PROM.BIN" "$work/assets" "$work/core-2503/core-config-derived.txt" \
  "$capture/handoff-test-09-windows-pass/2503-HANDOFF.TXT" "$capture/display-graph-02/IMG36.BIN" > "$out/frontend-results.txt"
cat "$out/frontend-results.txt"
python3 unlock_manifest.py frontend
