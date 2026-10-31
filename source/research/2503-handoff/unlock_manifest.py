"""Pin build/test inputs and record capability separately from hardware success."""
import hashlib
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parent
repo = root.parents[1]
out = root / 'build-unlock'
capture = repo.parent / 'rtx3060-capture-20261030-141202'
assets = repo.parent / 'rtx3060-unlock-assets/assets'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()

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
elif mode == 'frontend':
    source_hashes = inputs()
    assert json.loads((out / 'frontend-inputs-before.json').read_text()) == source_hashes
    log = (out / 'frontend-results.txt').read_text()
    assert re.findall(r'^PASS full adapter case (\d+):', log, re.M) == list(map(str, range(15)))
    assert log.splitlines()[-1] == 'PASS: 15 full-adapter cases with real core, captured AMI code, exact assets and RAM-only EFI'
    write(out / 'frontend-verification.json', {
        'all_passed': True, 'scenarios': 15, 'hardware_accessed': False,
        'test_executable_sha256': digest(out / 'test-core-frontend'),
        'result_sha256': digest(out / 'frontend-results.txt'), 'sources': source_hashes})
elif mode == 'finish':
    source_hashes = inputs()
    assert json.loads((out / 'build-inputs-before.json').read_text()) == source_hashes
    frontend = json.loads((out / 'frontend-verification.json').read_text())
    assert frontend['all_passed'] and frontend['scenarios'] == 15 and frontend['sources'] == source_hashes
    assert frontend['result_sha256'] == digest(out / 'frontend-results.txt')
    pe = json.loads((out / 'pe-verification.json').read_text())
    assert pe['sha256'] == digest(out / 'UNLOCK.EFI') and pe['imports'] == 0
    link_map = (out / 'UNLOCK.map').read_text()
    for symbol in ['gpu_unlock_610_unlock', 'GuEnableMaster', 'GuFlr', 'UbPrepareWindows', 'efi_main']:
        assert re.search(r'\b' + symbol + r'\b', link_map), symbol
    specs = json.loads((repo / 'internal-boot/unlock/assets.json').read_text(encoding='utf-8-sig'))
    for row in specs:
        blob = assets / row['Name']
        assert blob.stat().st_size == row['Bytes'] and digest(blob).lower() == row['Sha256'].lower()
    assert digest(capture / 'PROM.BIN') == '023fc329d52741164ccb1632206883b87a778af6f5f8532a1dd5426fa09a13c5'
    boot = (out / 'boot-results.txt').read_text()
    counts = [27, 36, 40, 45, 7, 19, 9]
    assert [int(n) for n in re.findall(r'^PASS: (\d+) ', boot, re.M)] == counts
    core = json.loads((repo.parent / 'rtx3060-unlock-assets/core-2503/result.json').read_text())
    assert core['core_model_scenarios_passed'] == 252 and core['reviewed_dma_scenarios_passed'] == 14
    assert core['model_firmware_build_rejected'] and core['reviewed_fixture_compile_passed']
    write(out / 'manifest.json', {
        'TestVersion': 10, 'Mode': 'experimental-own-X99-GA106-2503-full-core-to-Windows',
        'CanUnlock': True, 'HardwareTested': False, 'UnlockSucceeded': False,
        'PriorHandoffVersion': 9, 'PriorVisibleWindowsHandoffVerified': True,
        'ManualActivationKey': 'U', 'TimeoutSeconds': 15, 'NoKeyAction': 'start Windows without unlock',
        'ExecutesFirmware': True, 'EnablesBusMaster': True, 'ResetsGpu': True,
        'WritesUnlockSelectors': True, 'WritesVbiosFlash': False, 'WritesFirmwareVariables': False,
        'ReconnectsGopAfterCore': False, 'RestoresEntryVgaDecode': True,
        'WindowsDeviceRestartEnabled': False, 'TargetPci': '10de:2503', 'Subsystem': '10de:2503',
        'Bar0': '0xfa000000', 'PciLocation': '0000:07:00.0',
        'PromSha256': digest(capture / 'PROM.BIN'), 'EfiSha256': pe['sha256'],
        'Assets': specs, 'BootScenariosPassed': sum(counts), 'CoreScenariosPassed': 252,
        'FullAdapterScenariosPassed': 15, 'DmaPortScenariosPassed': 14,
        'FrontendResultsSha256': frontend['result_sha256'],
        'BootResultsSha256': digest(out / 'boot-results.txt'),
        'Sources': source_hashes,
        'HardwareSuccessMustBeEstablishedFrom': 'new USB log, visible Windows, healthy driver and matched benchmark',
    })
else:
    raise ValueError(mode)
print('PASS manifest phase:', mode)
