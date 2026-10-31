#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# RAM-only tests; no firmware files, device access or boot configuration changes.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
out="$root/build/public-tests"
mkdir -p "$out"
cd "$root/source/research/2503-handoff"
gcc -std=c11 -fshort-wchar -Wall -Wextra -Werror -Wno-misleading-indentation \
  -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  test_auto_policy.c -o "$out/test-auto-policy"
"$out/test-auto-policy" > "$out/policy-results.txt"
gcc -std=c11 -DHT_INTERNAL_AUTO -fshort-wchar -Wall -Wextra -Werror -Wno-misleading-indentation \
  -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  test_windows.c -o "$out/test-auto-windows"
"$out/test-auto-windows" > "$out/windows-results.txt"
cat "$out/policy-results.txt" "$out/windows-results.txt"
