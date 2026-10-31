#!/bin/sh
set -eu
cd "$(dirname "$0")"
capture=$1
mkdir -p build-unlock
sh test.sh "$capture/display-graph-02/IMG36.BIN" \
  "$capture/handoff-test-09-windows-pass/2503-HANDOFF.TXT" "$capture/PROM.BIN" > build-unlock/boot-results.txt
cat build-unlock/boot-results.txt
# The independent firmware profile must not compile without the full adapter.
core=../../internal-boot/unlock
if gcc -std=c11 -ffreestanding -fshort-wchar -DGU_RESEARCH_2503_FIRMWARE \
  -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  -c "$core/port.c" -o build-unlock/forbidden-firmware.o 2> build-unlock/incomplete-profile-rejected.txt; then
  echo 'ERROR: incomplete 2503 firmware profile compiled' >&2
  exit 1
fi
python3 -c 'assert "requires its standalone full adapter" in open("build-unlock/incomplete-profile-rejected.txt").read()'
echo 'PASS: incomplete private firmware profile rejected'
