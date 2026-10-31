#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Prepare pinned local GA10x inputs. Downloads only with --download; no install."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tarfile
import urllib.request

# Keep interpreter caches (which embed local paths) out of the public tree.
sys.dont_write_bytecode = True

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--cache', type=Path, default=root/'local/cache')
p.add_argument('--output', type=Path, default=root/'local/firmware')
p.add_argument('--download', action='store_true')
a = p.parse_args()
manifest = json.loads((root/'firmware/upstream/artifacts-610.43.03.json').read_text())

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024), b''): h.update(b)
    return h.hexdigest()

packages = []
for index in [0, 2]:
    row = manifest['source_artifacts'][index]
    path = a.cache/row['filename']
    if not path.exists():
        if not a.download:
            raise SystemExit(f'Missing {row["filename"]}; supply --cache or use --download')
        a.cache.mkdir(parents=True, exist_ok=True)
        part = path.with_name(path.name+'.part')
        print('Downloading', row['url'], flush=True)
        with urllib.request.urlopen(row['url'], timeout=120) as src, part.open('wb') as dst:
            while True:
                block = src.read(1024*1024)
                if not block: break
                dst.write(block)
        if digest(part) != row['sha256']: raise SystemExit('Downloaded artifact hash mismatch')
        part.replace(path)
    if digest(path) != row['sha256']: raise SystemExit('Artifact hash mismatch: '+path.name)
    packages.append(path)

try: import zstandard
except ImportError: raise SystemExit('Install requirements-firmware.txt before preparing assets')

def module(name):
    spec = importlib.util.spec_from_file_location(name, root/'firmware/upstream'/f'{name}.py')
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

extractor, generator = module('extract_boot_assets'), module('generate_v67')
names = ['g_bindata_kgspGetBinArchiveBooterLoadUcode_GA102.c', 'g_bindata_kgspGetBinArchiveGspRmBoot_GA102.c']
source = a.output.parent/'extracted-source'
with tarfile.open(packages[0], 'r:*') as archive:
    for name in names:
        matches = [m for m in archive.getmembers() if m.name.endswith('/src/nvidia/generated/'+name)]
        if len(matches) != 1 or not matches[0].isfile() or matches[0].size > 16*1024*1024:
            raise SystemExit('Missing, duplicated or unsafe generated source')
        target = source/'src/nvidia/generated'/name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(archive.extractfile(matches[0]).read())
extractor.extract_assets(source, a.output, 'ga102')

# Stream only exact package members. Never unpack arbitrary archive paths.
def member(name):
    with packages[1].open('rb') as raw:
        with zstandard.ZstdDecompressor().stream_reader(raw) as stream:
            with tarfile.open(fileobj=stream, mode='r|') as archive:
                return generator.read_member(archive, name)
metadata = member('.PKGINFO').decode('utf-8', errors='replace')
if 'pkgver = 610.43.03-1' not in metadata.splitlines(): raise SystemExit('Package version mismatch')
(a.output/'gsp_ga10x.bin').write_bytes(member(generator.FIRMWARE_PREFIX+'gsp_ga10x.bin'))
(a.output/'v67-ga102-610.43.03.bin').write_bytes(generator.build_payload())
notice = manifest['source_artifacts'][2]['notice']
license_text = member(notice['path'])
if hashlib.sha256(license_text).hexdigest() != notice['sha256']: raise SystemExit('License notice mismatch')
(a.output/'NVIDIA-LICENSE').write_bytes(license_text)
specs = json.loads((root/'source/internal-boot/unlock/assets.json').read_text())
for row in specs:
    f = a.output/row['Name']
    if f.stat().st_size != row['Bytes'] or digest(f) != row['Sha256']:
        raise SystemExit('Derived asset mismatch: '+f.name)
report = {'PinnedInputsVerified': True, 'HardwareAccessed': False, 'Installed': False, 'Assets': specs}
(a.output/'verified-assets.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
print('PASS: all five exact assets and NVIDIA license prepared locally. No installation.')
