"""Separate internal-auto artifact; the proven USB image remains unchanged."""
import hashlib
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parent
repo = root.parents[1]
out = root / 'build-auto'
capture = repo.parent / 'rtx3060-capture-20261030-141202'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
def object_digest(path):
    data = path.read_bytes()
    assert data[:2] == b'\x64\x86' and len(data) >= 20
    # COFF TimeDateStamp is build metadata, not instructions/relocations/data.
    return hashlib.sha256(data[:4] + b'\0' * 4 + data[8:]).hexdigest()

def inputs():
    paths = [p for p in root.iterdir() if p.suffix in ('.c', '.h', '.py', '.sh')]
    core = repo / 'internal-boot/unlock'
    paths += [p for p in core.rglob('*') if p.is_file() and p.suffix in ('.c', '.h')
              and not any(x.startswith('build') for x in p.relative_to(core).parts)]
    paths += [repo / 'internal-boot/boot/media.h', repo / 'internal-boot/boot/platform-config.h']
    return {str(p.relative_to(repo)).replace('\\', '/'): digest(p) for p in sorted(set(paths))}

def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')

mode = sys.argv[1]
if mode == 'snapshot':
    write(Path(sys.argv[2]), inputs())
elif mode == 'tests':
    sources = inputs()
    assert json.loads((out / 'test-inputs-before.json').read_text()) == sources
    policy = (out / 'policy-results.txt').read_text()
    windows = (out / 'windows-results.txt').read_text()
    frontend = (out / 'frontend-results.txt').read_text()
    assert 'PASS: 21 automatic activation and pending-state scenarios' in policy
    assert 'PASS: 7 Windows loader cases' in windows
    assert re.findall(r'^PASS full adapter case (\d+):', frontend, re.M) == list(map(str, range(15)))
    assert 'PASS: 4 internal source-media rejection scenarios' in frontend
    write(out / 'tests.json', {'AllPassed': True, 'Scenarios': 47, 'HardwareAccessed': False,
                              'Sources': sources,
                              'Results': {n: digest(out / n) for n in ('policy-results.txt', 'windows-results.txt', 'frontend-results.txt')}})
elif mode == 'finish':
    sources = inputs()
    assert json.loads((out / 'build-inputs-before.json').read_text()) == sources
    tests = json.loads((out / 'tests.json').read_text())
    assert tests['AllPassed'] and tests['Sources'] == sources
    for name, sha in tests['Results'].items():
        assert digest(out / name) == sha
    pe = json.loads((out / 'pe-verification.json').read_text())
    assert pe['sha256'] == digest(out / 'AUTO.EFI') and not pe['imports']
    parity = {}
    for name in ('transaction', 'dma_probe', 'core_bridge', 'runtime', 'sha256', 'format', 'checks', 'port', 'compat',
                 'unlock_610', 'prep_610', 'layout_610', 'vbios_610', 'gfw_eval'):
        sha = object_digest(out / (name + '.obj'))
        assert sha == object_digest(root / 'build-unlock' / (name + '.obj')), 'Core object changed: ' + name
        parity[name] = sha
    assert b'2503-AUTO-PENDING-v11' in (out / 'AUTO.EFI').read_bytes()
    proof = json.loads((capture / 'unlock-test-10-20261031-140814/comparison.json').read_text(encoding='utf-8-sig'))
    assert proof['CoreSucceeded'] and proof['PerformanceImprovementVerified']
    training = json.loads((capture / 'stability-and-auto-20261031/training-stability-evidence.json').read_text(encoding='utf-8-sig'))
    assert training['UserReportedTrainingAccuracyNormal'] and training['MatchingSystemErrorCount'] == 0
    specs = json.loads((repo / 'internal-boot/unlock/assets.json').read_text())
    for spec in specs:
        file = repo.parent / 'rtx3060-unlock-assets/assets' / spec['Name']
        assert file.stat().st_size == spec['Bytes'] and digest(file) == spec['Sha256']
    write(out / 'manifest.json', {
        'TestVersion': 11, 'Mode': 'captured-2503-internal-automatic-to-original-Windows',
        'CanUnlock': True, 'Automatic': True, 'SkipWindowSeconds': 5, 'AnyKeySkipsUnlock': True,
        'HardwareTested': False, 'PriorUsbVersion10HardwareVerified': True,
        'EfiSha256': pe['sha256'], 'PriorUsbEfiSha256': proof['EfiSha256'],
        'WindowsEspGuid': '9d69eaa3-1dea-4abf-8c4b-236d216b0d80',
        'EfiPath': '\\EFI\\Codex2503\\AUTO.EFI', 'LogPath': '\\EFI\\Codex2503\\LASTRUN.TXT',
        'PendingPath': '\\EFI\\Codex2503\\PENDING.TXT', 'DisabledPath': '\\EFI\\Codex2503\\DISABLED.TXT',
        'FailedOrUnconfirmedPriorRunSkipsUnlock': True, 'WindowsMustConfirmBeforeRearm': True,
        'RuntimeWritesFirmwareVariables': False, 'ReplacesWindowsBootManager': False,
        'CoreObjectParityCount': len(parity), 'CoreObjectsUnchangedFromVerifiedUsb': parity,
        'CoreObjectComparisonExcludesOnlyCoffTimestamp': True,
        'ScenariosPassed': tests['Scenarios'], 'TrainingReport': training,
        'Assets': specs, 'Sources': sources})
else:
    raise ValueError(mode)
print('PASS auto manifest phase:', mode)
