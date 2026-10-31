#!/bin/sh
# Real transaction core, compatibility and EFI port; RAM-backed services only.
set -eu
cd "$(dirname "$0")"
capture=$1
work=$2
log=${3:-"$capture/handoff-test-09-windows-pass/2503-HANDOFF.TXT"}
out="$work/core-2503"
mkdir -p "$out"
python3 prepare_core_fixture.py "$capture" "$work" "$out" "$log"
core=../../internal-boot/unlock
gcc -std=c11 -O1 -D_POSIX_C_SOURCE=200112L -D__KERNEL__ -DGU_UEFI_PORT \
  -DGU_RESEARCH_2503_HOST -DGU_EXPERIMENTAL_3090 -DGU_ENTRY_LATCHED_SS1_FULL \
  -fshort-wchar -Wall -Wextra -Werror -Wno-unused-parameter \
  -Wno-unused-function -Wno-misleading-indentation \
  -I "$core/compat" -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  core_bridge.c "$core/tests/test_efi_transaction.c" "$core/checks.c" "$core/port.c" "$core/compat.c" \
  "$core/vendor/unlock_610.c" "$core/vendor/prep_610.c" \
  "$core/vendor/layout_610.c" "$core/vendor/vbios_610.c" "$core/vendor/gfw_eval.c" \
  -o "$out/test-core"
"$out/test-core" "$capture/PROM.BIN" "$work/assets" "$out/core-config-derived.txt" \
  "$log" > "$out/scenarios.txt"
tail -n 3 "$out/scenarios.txt"
sh test_core_guards.sh "$out"
python3 record_core_result.py "$capture" "$work"
