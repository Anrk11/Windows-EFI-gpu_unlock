#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Compile the captured-machine EFI; never installs it or accesses a GPU."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('mode', choices=['usb', 'auto'])
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
source = root / 'source'
cwd = source / 'research/2503-handoff'
out = root / 'build' / a.mode
out.mkdir(parents=True, exist_ok=True)

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def executable(variable, names):
    candidate = os.environ.get(variable)
    if candidate:
        found = shutil.which(candidate)
        if found: return found
        raise SystemExit(f'{variable} executable not found')
    for name in names:
        found = shutil.which(name)
        if found: return found
    raise SystemExit(f'Set {variable} or install one of: {", ".join(names)}')

clang = executable('CLANG', ['clang-21', 'clang'])
lld = executable('LLD_LINK', ['lld-link-21', 'lld-link'])
core = '../../internal-boot/unlock'
files = ['main.c', 'transaction.c', 'dma_probe.c', 'core_bridge.c']
files += [f'{core}/{name}.c' for name in ['runtime', 'sha256', 'format', 'checks', 'port', 'compat',
    'vendor/unlock_610', 'vendor/prep_610', 'vendor/layout_610', 'vendor/vbios_610', 'vendor/gfw_eval']]
flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-ffreestanding', '-fno-builtin',
    '-fno-stack-protector', '-mno-red-zone', '-fshort-wchar', '-Os', '-Wall', '-Wextra', '-Werror',
    '-Wno-unused-parameter', '-D__KERNEL__', '-DGU_UEFI_PORT', '-DGU_EXPERIMENTAL_3090',
    '-DGU_ENTRY_LATCHED_SS1_FULL', '-DGU_RESEARCH_2503_FIRMWARE', '-DHT_EXECUTE_CORE',
    '-DHT_HEADLESS_WINDOWS_TEST', '-DHT_RESTORE_VGA_DECODE']
if a.mode == 'auto': flags.append('-DHT_INTERNAL_AUTO')
flags += ['-I', f'{core}/compat', '-I', '../../deps/edk2/MdePkg/Include',
          '-I', '../../deps/edk2/MdePkg/Include/X64']
inputs = {str(path.relative_to(source)).replace('\\', '/'): digest(path)
          for path in sorted(source.rglob('*')) if path.is_file() and path.suffix in ['.c', '.h']}
objects = []
for name in files:
    obj = out / (Path(name).stem + '.obj')
    subprocess.run([clang, *flags, '-c', name, '-o', str(obj)], cwd=cwd, check=True)
    objects.append(str(obj))
name = 'AUTO' if a.mode == 'auto' else 'UNLOCK'
image, linkmap = out / (name + '.EFI'), out / (name + '.map')
subprocess.run([lld, '/nologo', '/nodefaultlib', '/subsystem:efi_application', '/entry:efi_main',
    '/machine:x64', '/base:0', '/dynamicbase', '/nxcompat', '/timestamp:0', '/opt:ref', '/opt:icf',
    '/map:' + str(linkmap), '/out:' + str(image), *objects], cwd=cwd, check=True)
subprocess.run([os.sys.executable, str(source / 'internal-boot/unlock/verify_pe.py'), str(image),
    str(linkmap), '--output', str(out / 'pe-verification.json')], check=True)
assert all(digest(source / rel) == sha for rel, sha in inputs.items()), 'Source changed during build'
release = root / 'releases' / ('auto-v11' if a.mode == 'auto' else 'usb-v10')
expected = json.loads((release / 'manifest.json').read_text(encoding='utf-8-sig'))['EfiSha256']
actual = digest(image)
report = {'Mode': a.mode, 'Compiled': True, 'HardwareExecutedByBuild': False,
    'FullFixtureTestsRunByBuild': False, 'RebuiltSha256': actual, 'PublishedSha256': expected,
    'ByteIdenticalToHardwareVerifiedImage': actual == expected,
    'Compiler': subprocess.check_output([clang, '--version'], text=True).splitlines()[0],
    'Linker': subprocess.check_output([lld, '--version'], text=True).strip(), 'Sources': inputs}
(out / 'rebuild.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k != 'Sources'}, indent=2))
print('No installation or hardware execution performed.')
