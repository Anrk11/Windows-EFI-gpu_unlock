#!/bin/sh
# Host-only tests. No GPU or firmware interfaces are opened.
set -eu
cd "$(dirname "$0")"
mkdir -p build
gcc -std=c11 -fshort-wchar -Wall -Wextra -Werror \
  -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  test_transaction.c transaction.c -o build/test-transaction
./build/test-transaction
gcc -std=c11 -D_GNU_SOURCE -fshort-wchar -Wall -Wextra -Werror -Wno-misleading-indentation \
  -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  test_adapter.c transaction.c dma_probe.c ../../internal-boot/unlock/sha256.c -o build/test-adapter
./build/test-adapter "$@"
gcc -std=c11 -D_GNU_SOURCE -DHT_HEADLESS_WINDOWS_TEST -fshort-wchar -Wall -Wextra -Werror -Wno-misleading-indentation \
  -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  test_adapter.c transaction.c dma_probe.c ../../internal-boot/unlock/sha256.c -o build/test-adapter-headless
./build/test-adapter-headless "$@"
gcc -std=c11 -D_GNU_SOURCE -DHT_HEADLESS_WINDOWS_TEST -DHT_RESTORE_VGA_DECODE -fshort-wchar -Wall -Wextra -Werror -Wno-misleading-indentation \
  -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  test_adapter.c transaction.c dma_probe.c ../../internal-boot/unlock/sha256.c -o build/test-adapter-vga
./build/test-adapter-vga "$@"
gcc -std=c11 -fshort-wchar -Wall -Wextra -Werror -Wno-misleading-indentation \
  -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  test_windows.c -o build/test-windows
./build/test-windows
gcc -std=c11 -D_POSIX_C_SOURCE=200112L -fshort-wchar -Wall -Wextra -Werror \
  -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  test_dma_probe.c dma_probe.c -o build/test-dma-probe
./build/test-dma-probe
gcc -std=c11 -fshort-wchar -ffunction-sections -fdata-sections \
  -Wall -Wextra -Werror -Wno-misleading-indentation \
  -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 \
  test_headless_return.c -Wl,--gc-sections -o build/test-headless-return
./build/test-headless-return
