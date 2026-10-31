"""Pin real capture/assets for a host-only core execution model; no device access."""
import hashlib
import json
from pathlib import Path
import re
import sys

capture, work, output = map(Path, sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
digest = lambda data: hashlib.sha256(data).hexdigest()
prom = (capture / 'PROM.BIN').read_bytes()
assert digest(prom) == '023fc329d52741164ccb1632206883b87a778af6f5f8532a1dd5426fa09a13c5'
log_path = Path(sys.argv[4]) if len(sys.argv) == 5 else capture / 'handoff-test-07-dma-pass/2503-HANDOFF.TXT'
log = log_path.read_bytes()
approved_inputs = {
    'e4a6d4f84aef2b84569b0744df163fe099afc5f571afe110a29bb54644cc5ca8': 7,
    '5636ee989d6a7bf6be883f4878f7b629728fc56ea31fe566029e7d669bdf5d7a': 9,
}
assert digest(log) in approved_inputs
report = (capture / 'REPORT.TXT').read_bytes()
section = report.decode().split('[PCI:0000]', 1)[1].split('[PCI:', 1)[0]
pairs = [(int(a, 16), int(b, 16)) for a, b in re.findall(r'^config_([0-9a-f]{2})=0x([0-9a-f]+)', section, re.M)]
assert [o for o, v in pairs] == list(range(0, 256, 4))
config = [v for o, v in pairs]
assert config[0] == config[11] == 0x250310de and config[4] == 0xfa000000
assert config[1] & 7 == 7
# Derived released+decode-restored fixture. Only BME is cleared. This is
# explicitly NOT a newly collected raw configuration snapshot.
config[1] &= ~4
text = ''.join('config[%02x]=%08x %08x %08x %08x\n' % (i*4, *config[i:i+4]) for i in range(0, 64, 4))
(output / 'core-config-derived.txt').write_text(text)
catalog = Path(__file__).resolve().parents[2] / 'internal-boot/unlock/assets.json'
assets = json.loads(catalog.read_text(encoding='utf-8-sig'))
for asset in assets:
    data = (work / 'assets' / asset['Name']).read_bytes()
    assert len(data) == asset['Bytes'] and digest(data).lower() == asset['Sha256'].lower()
manifest = {'host_only': True, 'unlock_approved': False, 'hardware_executed': False,
            'report_sha256': digest(report), 'dma_log_sha256': digest(log),
            'boot_fixture_version': approved_inputs[digest(log)],
            'boot_fixture_path': str(log_path),
            'prom_sha256': digest(prom), 'pci_fixture': 'derived from original capture; only BME cleared',
            'assets_verified': len(assets)}
(output / 'core-inputs.json').write_text(json.dumps(manifest, indent=2)+'\n')
print('PASS: captured 2503 identity, PROM, DMA log and five pinned firmware assets')
