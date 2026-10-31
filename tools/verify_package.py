#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Verify the sealed public package offline. Never installs or modifies hardware."""
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root/'PACKAGE-MANIFEST.json').read_text(encoding='utf-8'))
errors = []
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
for rel, expected in manifest['Files'].items():
    p = root/rel
    if not p.is_file() or digest(p) != expected: errors.append('Missing or changed: '+rel)
for mode, name in [('usb-v10','UNLOCK.EFI'), ('auto-v11','AUTO.EFI')]:
    release = root/'releases'/mode
    original = json.loads((release/'manifest.json').read_text(encoding='utf-8-sig'))
    if digest(release/name) != original['EfiSha256']: errors.append('EFI mismatch: '+mode)
    if mode == 'auto-v11':
        for rel, sha in original['Sources'].items():
            path = root/'source'/rel
            if not path.is_file() or digest(path) != sha: errors.append('v11 corresponding source changed: '+rel)
for p in root.rglob('*'):
    if not p.is_file(): continue
    rel = p.relative_to(root)
    if rel.parts[0] in ['.git','build','local']: continue
    if p.suffix.lower() in ['.bin','.zst','.xz','.zip','.pem','.key','.pyc']:
        errors.append('Private or firmware/archive file in public tree: '+str(rel))
    if p.name in ['BCD-export','bootmgfw.efi','PROM.BIN','IMG36.BIN']:
        errors.append('System backup or private firmware in public tree: '+str(rel))
    if p.suffix.lower() in ['.md','.txt','.json','.ps1','.py','.sh','.c','.h']:
        data = p.read_text(encoding='utf-8-sig', errors='replace')
        # Account paths and raw PCI PnP instance suffixes are not publication inputs.
        if re.search(r'[A-Za-z]:[\\/]+Users[\\/]+[^\\/\s]+', data, re.I):
            errors.append('Personal account path: '+str(rel))
        if re.search(r'(?i)(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|sk-[A-Za-z0-9]{32,})', data):
            errors.append('Possible access credential: '+str(rel))
if errors:
    raise SystemExit('\n'.join(errors))
print(f'PASS: {len(manifest["Files"])} checksummed files; both EFI images; exact v11 sources; public-tree checks.')
print('Offline verification only; no boot changes, firmware downloads or GPU execution.')
