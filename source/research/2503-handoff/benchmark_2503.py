"""Matched Windows PyTorch GEMM measurements; no driver or GPU setting changes."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--label', choices=['stock', 'after-unlock-attempt'], required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if args.output.exists():
    raise RuntimeError('Preserve the existing measurement; choose a new output file')
if os.environ.get('NVIDIA_TF32_OVERRIDE'):
    raise RuntimeError('TF32 override is set; measurements would not match this protocol')

def smi(fields):
    result = subprocess.run(['nvidia-smi', '--query-gpu=' + fields,
                             '--format=csv,noheader,nounits'], check=True,
                            capture_output=True, text=True, timeout=15)
    rows = result.stdout.strip().splitlines()
    if len(rows) != 1:
        raise RuntimeError('Expected one NVIDIA GPU')
    return [value.strip() for value in rows[0].split(',')]

identity = smi('name,pci.device_id,pci.bus_id,driver_version,power.limit,clocks.max.sm,clocks.max.memory')
if identity[1] != '0x250310DE' or not identity[2].endswith(':07:00.0'):
    raise RuntimeError('This benchmark is pinned to this owner\'s 2503 at 07:00.0')
load = smi('utilization.gpu,memory.used,temperature.gpu')
if float(load[0]) > 30 or float(load[1]) > 2048 or float(load[2]) > 65:
    raise RuntimeError('GPU is busy or hot; existing applications were left running')

import torch
if not torch.cuda.is_available() or torch.cuda.device_count() != 1:
    raise RuntimeError('Expected one working CUDA device')
torch.set_num_threads(4)
torch.manual_seed(20261030)
torch.cuda.manual_seed_all(20261030)
torch.backends.cuda.matmul.allow_tf32 = False
torch.backends.cuda.matmul.allow_fp16_reduced_precision_reduction = False
torch.backends.cuda.matmul.allow_bf16_reduced_precision_reduction = False
if hasattr(torch.backends.cuda.matmul, 'allow_fp16_accumulation'):
    torch.backends.cuda.matmul.allow_fp16_accumulation = False
result = {
    'protocol': 1, 'label': args.label,
    'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'gpu_settings': identity, 'initial_load': load,
    'torch': torch.__version__, 'cuda': torch.version.cuda,
    'script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    'seed': 20261030, 'iterations': 10, 'trials': 3,
    'float32_output': True, 'reduced_precision_reduction': False,
    'tf32': False, 'desktop_active': True,
    'correctness_scope': 'top-left 16x16 tile against CPU float64 with full K',
    'measurements': [], 'complete': False,
}
for name, dtype in [('fp16_float32_output', torch.float16), ('bf16_float32_output', torch.bfloat16)]:
    for n in [4096, 8192]:
        a = torch.randn((n, n), device='cuda', dtype=dtype) * 0.01
        b = torch.randn((n, n), device='cuda', dtype=dtype) * 0.01
        out = torch.empty((n, n), device='cuda', dtype=torch.float32)
        torch.mm(a, b, out_dtype=torch.float32, out=out)
        reference = a[:16, :].cpu().double() @ b[:, :16].cpu().double()
        actual = out[:16, :16].cpu().double()
        torch.testing.assert_close(actual, reference, rtol=0.003, atol=0.0003)
        maximum_error = (actual - reference).abs().max().item()
        for _ in range(2):
            torch.mm(a, b, out_dtype=torch.float32, out=out)
        timings = []
        for _ in range(3):
            start, end = torch.cuda.Event(enable_timing=True), torch.cuda.Event(enable_timing=True)
            start.record()
            for _ in range(10):
                torch.mm(a, b, out_dtype=torch.float32, out=out)
            end.record()
            end.synchronize()
            timings.append(start.elapsed_time(end) / 10)
        tflops = [2 * n ** 3 / (ms / 1000) / 1e12 for ms in timings]
        row = {'mode': name, 'n': n, 'milliseconds': timings, 'tflops': tflops,
               'median_tflops': statistics.median(tflops), 'reference_checked': True,
               'maximum_tile_absolute_error': maximum_error}
        result['measurements'].append(row)
        print(name, n, 'median TF/s', round(row['median_tflops'], 3), flush=True)
        del a, b, out, actual, reference
torch.cuda.synchronize()
result['gpu_settings_after'] = smi('name,pci.device_id,pci.bus_id,driver_version,power.limit,clocks.max.sm,clocks.max.memory')
if result['gpu_settings_after'] != identity:
    raise RuntimeError('Comparison settings changed; run does not qualify')
result['final_load'] = smi('utilization.gpu,memory.used,temperature.gpu')
result['complete'] = True
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print('Saved', args.output)
