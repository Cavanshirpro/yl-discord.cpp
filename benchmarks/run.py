# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Cavanshirpro
# Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
"""Alternate fresh-process offline samples and save raw results plus medians."""
import argparse
import datetime
import json
import math
from pathlib import Path
import platform
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def sample(command, iterations, batch, implementation, version=None):
    result = subprocess.run([*command, str(iterations), str(batch)], cwd=ROOT,
                            capture_output=True, text=True, encoding='utf-8', timeout=120)
    if result.returncode:
        raise RuntimeError(f'Benchmark exited with {result.returncode}: {result.stderr[-2000:]}')
    data = json.loads(result.stdout)
    if data['implementation'] != implementation:
        raise ValueError('Benchmark implementation mismatch')
    if version is not None and data['version'] != version:
        raise ValueError('C++ benchmark version differs from VERSION; rebuild the executable')
    if data['iterations'] != iterations or data['batch_size'] != batch:
        raise ValueError('Benchmark configuration mismatch')
    values = [data['dispatch_seconds'], data['commands_per_second'], data['peak_resident_bytes'], *data['latency_us'].values()]
    if any(not math.isfinite(value) or value <= 0 for value in values):
        raise ValueError('Missing/nonfinite benchmark measurements')
    if not data['latency_us']['p50'] <= data['latency_us']['p95'] <= data['latency_us']['p99']:
        raise ValueError('Invalid latency percentiles')
    if data['implementation'] == 'yl-discord.cpp' and data['configuration'] != 'Release':
        raise ValueError('Use a Release C++ benchmark build')
    return data


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cpp', type=Path, required=True)
    parser.add_argument('--python', type=Path, default=Path(sys.executable))
    parser.add_argument('--iterations', type=int, default=10000)
    parser.add_argument('--batch-size', type=int, default=128)
    parser.add_argument('--repeats', type=int, default=5)
    parser.add_argument('--label', default='Local developer machine')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.iterations <= 200000 or not 1 <= args.batch_size <= 4096 or not 1 <= args.repeats <= 20:
        parser.error('Invalid count, batch or repeats')
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip())
    commands = {'cpp': [str(args.cpp.resolve())], 'python': [str(args.python.resolve()), str(ROOT / 'benchmarks/discord_py.py')]}
    expected = {'cpp': ('yl-discord.cpp', (ROOT / 'VERSION').read_text(encoding='utf-8').strip()),
                'python': ('discord.py', None)}
    results = {'cpp': [], 'python': []}
    for repeat in range(args.repeats):
        order = ('cpp', 'python') if repeat % 2 == 0 else ('python', 'cpp')
        for name in order:
            results[name].append(sample(commands[name], args.iterations, args.batch_size, *expected[name]))
    summary = {}
    for name, records in results.items():
        summary[name] = {'commands_per_second': statistics.median(r['commands_per_second'] for r in records),
                         'peak_resident_bytes': statistics.median(r['peak_resident_bytes'] for r in records),
                         'latency_us': {p: statistics.median(r['latency_us'][p] for r in records) for p in ('p50', 'p95', 'p99')}}
    report = {'schema': 1, 'workload': 'offline prebuilt-message dispatch -> prefix ping -> in-memory reply',
        'measured_at_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'label': args.label,
        'platform': platform.platform(), 'architecture': platform.machine(), 'source_commit': commit,
        'source_tree_dirty': dirty, 'iterations': args.iterations, 'batch_size': args.batch_size,
        'repeats': args.repeats, 'summary': summary, 'samples': results,
        'limitations': ['No Gateway, HTTP, TLS, rate limits, parsing benchmark or real Discord account.',
            'Input model construction and startup are excluded from dispatch timing; peak process memory includes startup.',
            'Both fixtures retain reply requests and message snapshots; model/features/cache implementations differ.',
            'This single-host synthetic workload is not an end-to-end or feature-equivalent bot comparison.']}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'source_commit': commit, 'source_tree_dirty': dirty, 'summary': summary}, indent=2))


if __name__ == '__main__':
    main()
