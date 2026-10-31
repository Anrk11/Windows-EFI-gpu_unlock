"""Record completed host tests and separately scoped hardware observations."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys

capture, work = (Path(arg).resolve() for arg in sys.argv[1:])
repo = Path(__file__).resolve().parents[2]
out = work / 'core-2503'
scenarios = (out / 'scenarios.txt').read_bytes()
rows = scenarios.decode().splitlines()
core = sum(bool(re.fullmatch(r'EFI integration .* PASS', row)) for row in rows)
handoff = sum(bool(re.fullmatch(r'Post-core Windows commit .* PASS', row)) for row in rows)
assert core == 180 and handoff == 72
assert rows[-1].startswith('252 real core + compat + UEFI port scenarios passed;')
assert 'PASS: 14 actual DMA-port scenarios;' in (out / 'default-port.txt').read_text()
assert (out / 'reviewed-test-compile.o').stat().st_size > 0
assert '2503 core model is forbidden in firmware builds' in (out / 'firmware-build-rejected.txt').read_text()
sources = ['research/2503-handoff/' + name for name in (
    'core_bridge.c', 'core_bridge.h', 'core_model.h', 'test_core.sh',
    'test_core_guards.sh', 'prepare_core_fixture.py', 'record_core_result.py')]
sources += ['internal-boot/unlock/' + name for name in (
    'machine-profile.h', 'tests/test_efi_transaction.c', 'port.c', 'compat.c',
    'checks.c', 'vendor/unlock_610.c', 'vendor/prep_610.c', 'vendor/layout_610.c',
    'vendor/vbios_610.c', 'vendor/gfw_eval.c')]
digest = lambda data: hashlib.sha256(data).hexdigest()
evidence = capture / 'handoff-test-09-windows-pass/windows-verification.json'
verification = json.loads(evidence.read_text(encoding='utf-8-sig'))
assert verification['UserConfirmedVisibleWindows']
assert verification['LogSha256'] == '5636ee989d6a7bf6be883f4878f7b629728fc56ea31fe566029e7d669bdf5d7a'
assert digest((evidence.parent / '2503-HANDOFF.TXT').read_bytes()) == verification['LogSha256']
assert verification['PresentGpu'][0]['ConfigManagerErrorCode'] == 0
assert verification['NvidiaSmiExitCode'] == 0 and not verification['UnlockExecuted']
target = out / 'result.json'
if target.exists() and not (out / 'result-before-v9.json').exists():
    shutil.copyfile(target, out / 'result-before-v9.json')
result = {
    'core_model_scenarios_passed': core + handoff,
    'core_only_scenarios_passed': core,
    'post_core_windows_commit_scenarios_passed': handoff,
    'reviewed_dma_scenarios_passed': 14,
    'reviewed_fixture_compile_passed': True,
    'model_firmware_build_rejected': True,
    'hardware_executed': False, 'unlock_performed': False,
    'unlock_approved': False, 'efi_core_adapter_integrated': False,
    'windows_without_gop_hardware_verified': True,
    'windows_handoff_evidence_scope': 'v9 without core execution or GPU reset',
    'windows_after_core_hardware_verified': False,
    'windows_evidence_path': str(evidence),
    'windows_evidence_sha256': digest(evidence.read_bytes()),
    'log_sha256': digest(scenarios),
    'sources': {name: digest((repo / name).read_bytes()) for name in sources},
    'device_and_dma_mask_lifetime_stored_in_context': True,
    'windows_vga_restore_is_one_shot_after_complete_cleanup': True,
}
target.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print('PASS: 252 host core/Windows commit cases, 14 DMA cases, build guards and scoped v9 evidence recorded')
