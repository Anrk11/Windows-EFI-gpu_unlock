"""Prepare pinned local firmware inputs. Never installs an EFI or touches a GPU."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tarfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('upstream', type=Path)
p.add_argument('work', type=Path)
a = p.parse_args()
def digest_file(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()
manifest = json.loads((a.upstream / 'firmware/artifacts-610.43.03.json').read_text())
cache = a.work / 'cache'
source = a.work / 'source'
assets = a.work / 'assets'
for index in (0, 2):
    row = manifest['source_artifacts'][index]
    blob = cache / row['filename']
    if digest_file(blob) != row['sha256']:
        raise ValueError('Input artifact hash mismatch: ' + blob.name)
names = ('g_bindata_kgspGetBinArchiveBooterLoadUcode_GA102.c', 'g_bindata_kgspGetBinArchiveGspRmBoot_GA102.c')
with tarfile.open(cache / manifest['source_artifacts'][0]['filename']) as archive:
    for name in names:
        members = [m for m in archive if m.name.endswith('/src/nvidia/generated/' + name)]
        if len(members) != 1 or not members[0].isfile() or members[0].size > 16 * 1024 * 1024:
            raise ValueError('Missing or unsafe pinned generated source')
        target = source / 'src/nvidia/generated' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(archive.extractfile(members[0]).read())
subprocess.run([sys.executable, str(a.upstream / 'firmware/extract_boot_assets.py'), str(source), str(assets)], check=True)
# Keep the original verified extraction/generation logic; supply only a local
# decompression reader so the standalone zstd executable is not required.
sys.path.insert(0, str(a.work / 'pythonlibs'))
import zstandard
spec = importlib.util.spec_from_file_location('pinned_generate', a.upstream / 'firmware/generate_v67.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
def extract_member(package, name):
    with package.open('rb') as raw:
        with zstandard.ZstdDecompressor().stream_reader(raw) as decoded:
            with tarfile.open(fileobj=decoded, mode='r|') as archive:
                return generator.read_member(archive, name)
generator.extract_member = extract_member
sys.argv = ['generate_v67.py', str(cache / manifest['source_artifacts'][2]['filename']), str(assets)]
if generator.main():
    raise ValueError('Pinned generation failed')
specs = json.loads((Path(__file__).resolve().parents[2] / 'internal-boot/unlock/assets.json').read_text())
checked = []
for row in specs:
    blob = assets / row['Name']
    digest = digest_file(blob)
    if blob.stat().st_size != row['Bytes'] or digest != row['Sha256']:
        raise ValueError('Derived asset mismatch: ' + blob.name)
    checked.append(dict(row))
(a.work / 'verified-assets.json').write_text(json.dumps({'can_unlock': False, 'hardware_execution_validated': False, 'assets': checked}, indent=2))
print('Verified all five firmware inputs; local files only, no EFI installation.')
