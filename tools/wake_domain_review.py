#!/usr/bin/env python3
"""Run/collect independent accurate C domains; check fixed gates and plot their crops."""
import argparse
import concurrent.futures
import json
import math
from pathlib import Path
import struct
import subprocess
import zlib


def run_case(executable, output, case):
    with (output / f'case-{case}.json').open('w') as report, \
         (output / f'case-{case}.log').open('w') as log:
        subprocess.run([str(executable), '--domain-case', str(case),
                        str(output / f'case-{case}.ppm')],
                       stdout=report, stderr=log, check=True)


def plots(output):
    images = []
    for case in range(6):
        with (output / f'case-{case}.ppm').open('rb') as source:
            if source.readline() != b'P6\n':
                raise ValueError('expected binary PPM')
            width, height = map(int, source.readline().split())
            if source.readline() != b'255\n' or (width, height) != (96, 320):
                raise ValueError('unexpected crop dimensions')
            pixels = source.read()
            if len(pixels) != width * height * 3:
                raise ValueError('incomplete image')
            images.append(pixels)
    width, height = 96 * 6, 320
    pixels = b''.join(images[case][row * 96 * 3:(row + 1) * 96 * 3]
                      for row in range(height) for case in range(6))
    raw = b''.join(b'\0' + pixels[row * width * 3:(row + 1) * width * 3]
                   for row in range(height))

    def chunk(kind, data):
        return (struct.pack('!I', len(data)) + kind + data +
                struct.pack('!I', zlib.crc32(kind + data)))

    (output / 'comparison.png').write_bytes(
        b'\x89PNG\r\n\x1a\n' +
        chunk(b'IHDR', struct.pack('!2I5B', width, height, 8, 2, 0, 0, 0)) +
        chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
    labels = ['144m open', '144m island', '144m wall',
              '288m open', '288m island', '288m wall']
    svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="672" height="390">',
           '<rect width="672" height="390" fill="#101b29"/>',
           '<g font-family="monospace" font-size="10" fill="white">',
           '<text x="8" y="15">W2.1 accurate: dx .75m, order 256, dt 1/120s; eta +/- .03m</text>',
           '<image x="48" y="44" width="576" height="320" href="comparison.png"/>']
    for i, label in enumerate(labels):
        svg.append(f'<text x="{48 + i * 96}" y="37">{label}</text>')
    for i, time in enumerate((4, 8, 12, 40, 80)):
        svg.append(f'<text x="5" y="{78 + i * 64}">{time}s</text>')
    svg += ['<text x="48" y="382">Each column: central 72x48m, same world coordinates and color scale.</text>',
            '</g></svg>']
    (output / 'comparison.svg').write_text('\n'.join(svg) + '\n')


def collect(output):
    cases = [json.loads((output / f'case-{i}.json').read_text()) for i in range(6)]
    failed = []
    for i, case in enumerate(cases):
        expected = {'case': i, 'kind': i % 3, 'spacing': .75, 'order': 256,
                    'spatial_order': 8, 'ticks': 9600,
                    'width_m': 144 if i < 3 else 288,
                    'height_m': 96 if i < 3 else 192}
        if any(case.get(key) != value for key, value in expected.items()) or \
           not math.isclose(case.get('dt', 0), 1 / 120, rel_tol=0, abs_tol=1e-17):
            failed.append(f'case {i}: accurate configuration mismatch')
        if len(case['roi']) != 3 or any(len(row) != 1024 for row in case['roi']):
            raise ValueError(f'case {i}: missing region samples')
        numeric = [v for row in case['roi'] for v in row] + [
            case[key] for key in ('initial_energy', 'normalization', 'retained_fraction',
                                 'absorbed_fraction', 'residual_fraction', 'early_shadow',
                                 'late_shadow', 'return_shadow')]
        if not all(math.isfinite(v) for v in numeric):
            failed.append(f'case {i}: nonfinite state/accounting')
        if case['normalization'] <= 0 or case['initial_energy'] <= 0:
            raise ValueError(f'case {i}: unexcited source')
        if case['absorbed_fraction'] < 0 or abs(case['residual_fraction']) >= .01:
            failed.append(f'case {i}: invalid sink or >1% propagation residual')
        if i < 3 and not 0 <= case['retained_fraction'] < .01:
            failed.append(f'case {i}: retains >=1% energy at 80s')
        if i % 3 == 2 and any(case[key] != 0 for key in
                             ('early_shadow', 'late_shadow', 'return_shadow')):
            failed.append(f'case {i}: wall transmission')
    differences = []
    for sample, time in enumerate((12, 40, 80)):
        row = {'time': time}
        for kind, name in ((0, 'open'), (1, 'island')):
            a, b = cases[kind], cases[kind + 3]
            if not math.isclose(a['normalization'], b['normalization'], rel_tol=1e-12):
                failed.append(f'{name}: domain source normalization differs')
            error = math.sqrt(sum((x - y)**2 for x, y in zip(a['roi'][sample], b['roi'][sample])) /
                              a['normalization'])
            row[name] = error
            if not math.isfinite(error) or error >= .01:
                failed.append(f'{name} at {time}s: domain region difference >=1%')
        differences.append(row)
    ratios = []
    for base in (0, 3):
        row = {'width_m': cases[base]['width_m']}
        for key in ('early_shadow', 'late_shadow'):
            denominator = cases[base][key]
            if denominator <= 0:
                raise ValueError('unexcited open-water probe')
            ratio = cases[base + 1][key] / denominator
            row[key] = ratio
            if ratio >= .01:
                failed.append(f'{row["width_m"]}m {key}: island/open intensity ratio >=1%')
        ratios.append(row)
    result = {'configuration': 'dx=.75, spatial=8, polynomial=256, dt=1/120, duration=80s',
              'region': 'central 24x24m, normalized by initial potential packet scale',
              'cases': [{k: v for k, v in case.items() if k != 'roi'} for case in cases],
              'domain_roi_difference': differences, 'island_ratios': ratios,
              'failed_gates': failed, 'domain_gates_passed': not failed}
    (output / 'measurements.json').write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
    plots(output)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, default=Path('build/wake_field_review_optimized'))
    parser.add_argument('--output', type=Path, default=Path('build/wake-w21-accurate'))
    parser.add_argument('--jobs', type=int, choices=range(1, 7), default=2)
    parser.add_argument('--collect-only', action='store_true')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    if not args.collect_only:
        executable = args.executable.resolve(strict=True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as workers:
            futures = [workers.submit(run_case, executable, args.output, i) for i in range(6)]
            for future in concurrent.futures.as_completed(futures):
                future.result()
    result = collect(args.output)
    print(json.dumps({'output': str(args.output), 'domain_roi_difference': result['domain_roi_difference'],
                      'failed_gates': result['failed_gates']}, indent=2))
    return int(bool(result['failed_gates']))


if __name__ == '__main__':
    raise SystemExit(main())
