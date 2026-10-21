#!/usr/bin/env python3
"""W1 connected-strip experiment. Standard library only; never loaded by gameplay.

Material coordinates are source time s and transverse distance q. Shared cubic Hermite
curves carry centre, normal and emission clock; independent strips add signed heights.
The Jacobian transports an action/area budget until a caustic. No amplitude clamp or
age fade repairs folds. This deliberately exposes the representation's rejection gate.
"""
import argparse
import cmath
import json
import math
from pathlib import Path
import subprocess
import struct
import zlib

G, RHO, K = 9.81, 1000.0, 2 * math.pi / 6
OMEGA = math.sqrt(G * K)
CP, CG = OMEGA / K, OMEGA / K / 2
HALF_WIDTH, AMPLITUDE = 12.0, 0.3  # four wavelengths across the full train


def cross(a, b):
    return (a.conjugate() * b).imag


def source(name, s):
    if name == 'loop':
        a = 8 * s / 12
        c, v = 12 * complex(math.sin(a), math.cos(a)), 8 * complex(math.cos(a), -math.sin(a))
    elif name == 's-turn':
        a = math.pi * (s / 6 - 1)
        c, v = complex(8 * s - 48, 12 * math.sin(a)), complex(8, 2 * math.pi * math.cos(a))
    elif name == 'spreading':
        a = 8 * s / 40
        c, v = 40 * cmath.exp(1j * a), 8j * cmath.exp(1j * a)
    else:
        c, v = complex(8 * s - 48, 0), 8 + 0j
    direction = v / abs(v)
    return c - 2 * direction, direction * 1j


def node(name, s):
    c, n = source(name, s)
    eps = 1e-4
    a, b = source(name, s - eps), source(name, s + eps)
    return s, c, (b[0] - a[0]) / (2 * eps), n, (b[1] - a[1]) / (2 * eps)


def hermite(a, da, b, db, u, length):
    value = (2*u**3-3*u*u+1)*a + (u**3-2*u*u+u)*length*da
    value += (-2*u**3+3*u*u)*b + (u**3-u*u)*length*db
    derivative = ((6*u*u-6*u)*a + (3*u*u-4*u+1)*length*da
                  + (-6*u*u+6*u)*b + (3*u*u-2*u)*length*db) / length
    return value, derivative


def interpolate(a, b, u):
    length = b[0] - a[0]
    c, dc = hermite(a[1], a[2], b[1], b[2], u, length)
    n, dn = hermite(a[3], a[4], b[3], b[4], u, length)
    return a[0] + u * length, c, dc, n, dn


class Front:
    def __init__(self, name, spacing=.4):
        self.nodes = [node(name, 12 * i / round(12 / spacing))
                      for i in range(round(12 / spacing) + 1)]
        # Relative action budget per parameter interval. SI source work is a W2 task.
        self.budgets = [b[0] - a[0] for a, b in zip(self.nodes, self.nodes[1:])]

    def split(self):
        nodes, budgets = [], []
        for a, b, energy in zip(self.nodes, self.nodes[1:], self.budgets):
            nodes.extend([a, interpolate(a, b, .5)])
            budgets.extend([energy / 2, energy / 2])
        self.nodes, self.budgets = nodes + [self.nodes[-1]], budgets


def profile(q, age):
    r = q / HALF_WIDTH
    if abs(r) >= 1:
        return 0., 0.
    e = (1-r*r)**3
    de = -6*q / HALF_WIDTH**2 * (1-r*r)**2
    phase = K*q - (OMEGA-K*CG)*age
    # Derivative of a compact potential guarantees zero transverse integral before
    # the curved-strip Jacobian/amplitude correction. Measure volume again afterwards.
    h = e * math.sin(phase) - de / K * math.cos(phase)
    quadrature = e * math.cos(phase) + de / K * math.sin(phase)
    return h, -G / OMEGA * quadrature


def evaluate(n, q, time, side):
    s, c, dc, normal, dn = n
    age = time - s
    d = .9 + CG*age + q
    p = c + side*normal*d
    ds, dq = dc + side*(dn*d-normal*CG), side*normal
    original = cross(dc, dq)
    jac = cross(ds, dq)
    ratio = jac / original
    if ratio <= 0 or age < 0:
        return p, 0., 0., ratio, abs(jac)
    taper = math.sin(math.pi*s/12)**2
    amplitude = AMPLITUDE * taper / math.sqrt(ratio)
    h, psi = profile(q, age)
    return p, amplitude*h, amplitude*psi, ratio, abs(jac)


def mesh(front, time, side, dq=.5, rotation=1+0j):
    cells = []
    nq = round(2 * HALF_WIDTH / dq)
    for a, b, budget in zip(front.nodes, front.nodes[1:], front.budgets):
        # A cell owns integrated material action. Splitting halves both budget and
        # parameter footprint, preserving the source density used by reconstruction.
        strength = math.sqrt(budget / (b[0]-a[0]))
        rows = []
        for n in (a, b):
            row = []
            for j in range(nq+1):
                p, h, psi, ratio, jac = evaluate(n, -HALF_WIDTH+j*2*HALF_WIDTH/nq, time, side)
                row.append((p*rotation, h*strength, psi*strength, ratio, jac))
            rows.append(row)
        cells.append(rows)
    return cells


def raster(rows, h, psi, size, extent):
    """Shared material edges rasterize once; overlap of folded strips remains diagnostic."""
    scale = size / (2*extent)
    folded = 0
    for a, b in zip(rows, rows[1:]):
        for j in range(len(a)-1):
            corners = a[j], b[j], b[j+1], a[j+1]
            if any(v[3] <= 0 for v in corners):
                folded += 1
                continue  # explicit rejection, not a valid physical hole treatment
            for indices in ((0, 1, 2), (0, 2, 3)):
                v = [corners[k] for k in indices]
                p = [complex((w[0].real+extent)*scale-.5,
                             (w[0].imag+extent)*scale-.5) for w in v]
                denom = cross(p[1]-p[0], p[2]-p[0])
                if abs(denom) < 1e-10:
                    continue
                for z in range(max(0, math.ceil(min(w.imag for w in p))),
                               min(size-1, math.floor(max(w.imag for w in p)))+1):
                    for x in range(max(0, math.ceil(min(w.real for w in p))),
                                   min(size-1, math.floor(max(w.real for w in p)))+1):
                        point = complex(x, z)-p[0]
                        u = cross(point, p[2]-p[0])/denom
                        w = cross(p[1]-p[0], point)/denom
                        # Half-open edges avoid counting shared vertices/edges twice.
                        if (u > 0 if indices == (0, 1, 2) else u >= 0) and w >= 0 and u+w < 1:
                            k = z*size+x
                            h[k] += (1-u-w)*v[0][1]+u*v[1][1]+w*v[2][1]
                            psi[k] += (1-u-w)*v[0][2]+u*v[1][2]+w*v[2][2]
    return folded


def field(name, time, size=128, extent=96., spacing=.4, dq=.5, sources=None):
    h, psi = [0.]*(size*size), [0.]*(size*size)
    folded = 0
    front = Front(name, spacing)
    for rotation in (sources if sources is not None else
                     ([1+0j, 1j] if name == 'crossing' else [1+0j])):
        for side in (-1, 1):
            for rows in mesh(front, time, side, dq, rotation):
                folded += raster(rows, h, psi, size, extent)
    return h, psi, folded


def fft(values):
    n = len(values)
    if n == 1:
        return values
    even, odd = fft(values[::2]), fft(values[1::2])
    result = [0j]*n
    for k in range(n//2):
        v = cmath.exp(-2j*math.pi*k/n)*odd[k]
        result[k], result[k+n//2] = even[k]+v, even[k]-v
    return result


def energy(h, psi, size, extent):
    """Linear deep-water Hamiltonian: rho/2 integral(g eta² + psi |D| psi).

    Surface potential is assigned by a narrow-band quadrature, not evolved by the
    exact free-surface equations. This audit can therefore reject the strip transport.
    The periodic FFT box must contain the full support; callers check/refine it.
    """
    rows = [fft(psi[z*size:(z+1)*size]) for z in range(size)]
    kinetic = 0.
    for x in range(size):
        column = fft([row[x] for row in rows])
        kx = min(x, size-x)*math.pi/extent
        for z, value in enumerate(column):
            kz = min(z, size-z)*math.pi/extent
            kinetic += math.hypot(kx, kz)*abs(value)**2/(size*size)
    area = (2*extent/size)**2
    return RHO/2*area*(G*sum(v*v for v in h)+kinetic)


def check():
    # One Fourier mode has an analytic kinetic + potential energy on a periodic box.
    size, extent = 8, 4.
    h = [math.sin(2*math.pi*x/size) for z in range(size) for x in range(size)]
    psi = [math.cos(2*math.pi*x/size) for z in range(size) for x in range(size)]
    expected = RHO/2*(2*extent)**2/2*(G+math.pi/extent)
    assert abs(energy(h, psi, size, extent)/expected-1) < 1e-12
    # A constant quad must cover every interior sample once, including its diagonal.
    rows = [[(complex(x, z), 1., 1., 1., 1.) for z in (-4., 4.)] for x in (-4., 4.)]
    h, psi = [0.]*64, [0.]*64
    assert raster(rows, h, psi, 8, 4.) == 0
    assert h == [1.]*64 and psi == [1.]*64
    # Declared numerical tolerances; failure of a physical gate is reported separately.
    assert abs(CP-2*CG) < 1e-12
    for name in ('straight', 'loop', 's-turn', 'spreading'):
        front = Front(name)
        before = sum(front.budgets)
        samples = [interpolate(a, b, .37) for a, b in zip(front.nodes, front.nodes[1:])]
        front.split()
        assert abs(sum(front.budgets)-before) < 1e-12
        for i, sample in enumerate(samples):
            refined = interpolate(front.nodes[2*i], front.nodes[2*i+1], .74)
            assert max(abs(a-b) for a, b in zip(sample, refined)) < 1e-9
            for side in (-1, 1):
                a, b = evaluate(sample, 1.7, 16, side), evaluate(refined, 1.7, 16, side)
                assert max(abs(x-y) for x, y in zip(a, b)) < 1e-8
    # The stored material budget actually supplies the reconstructed amplitude.
    front = Front('straight')
    rows = mesh(front, 16, 1)[0]
    front.budgets[0] *= 4
    stronger = mesh(front, 16, 1)[0]
    assert max(abs(v[1]*2-w[1]) for a, b in zip(rows, stronger)
               for v, w in zip(a, b)) < 1e-12
    # Compact derivative profile has zero integral, including changing carrier phase.
    for age in (0, 3, 60):
        dq = .01
        assert abs(sum(profile(-12+(i+.5)*dq, age)[0] * dq
                       for i in range(2400))) < 1e-8
    n = node('straight', 6)
    a = evaluate(n, 0, 16, 1)[0]
    b = evaluate(n, 0, 17, 1)[0]
    assert abs(abs(b-a)-CG) < 1e-10
    # Track a constant carrier phase in material q; world travel must be cp.
    a = evaluate(n, 0, 16, 1)[0]
    b = evaluate(n, (CP-CG)*.1, 16.1, 1)[0]
    assert abs(abs(b-a)/.1-CP) < 1e-9
    # Tight inner fronts must be detected as invalid, never silently amplitude-clamped.
    assert any(evaluate(n, 0, 24, side)[3] <= 0
               for n in Front('loop').nodes for side in (-1, 1))
    # Signed independent fields reinforce/cancel exactly; test full finite rotated trains.
    a, p, _ = field('crossing', 16, 32, sources=[1+0j])
    b, q, _ = field('crossing', 16, 32, sources=[1j])
    c, r, _ = field('crossing', 16, 32)
    assert max(abs(x+y-z) for x, y, z in zip(a, b, c)) < 1e-12
    assert max(abs(x+y-z) for x, y, z in zip(p, q, r)) < 1e-12
    assert all(x + (-x) == 0 for x in a)
    print('W1 numerical checks passed; this does not accept the physical representation.')


def baseline(exe, name, time, points):
    result = subprocess.run([str(exe), name, str(time)],
                            input=''.join(f'{p.real:.8f} {p.imag:.8f}\n' for p in points),
                            text=True, capture_output=True, check=True)
    rows = [list(map(float, line.split())) for line in result.stdout.splitlines()]
    if len(rows) != len(points) or any(len(row) != 4 for row in rows):
        raise RuntimeError('incomplete production sampler output')
    return rows


def gradient(h, size, step):
    result = [0.]*len(h)
    for z in range(1, size-1):
        for x in range(1, size-1):
            i = z*size+x
            result[i] = math.hypot(h[i+1]-h[i-1], h[i+size]-h[i-size])/(2*step)
    return result


def svg(path, name, frames, size, extent):
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="1100" height="1050" viewBox="0 0 1100 1050">',
           '<rect width="1100" height="1050" fill="#101b29"/>',
           '<g font-family="monospace" font-size="13" fill="white">',
           f'<text x="20" y="25">W1 {name}: old / connected strip; height +/-0.6 m; slope 0..0.5</text>']
    colors = []
    for i in range(256):
        v = (i-128)/128
        colors.append(f'#{int(40+max(v,0)*210):02x}{int(80+abs(v)*100):02x}{int(120+max(-v,0)*130):02x}')
    for col, (time, old, new, folded) in enumerate(frames):
        x0 = 20+col*360
        out.append(f'<text x="{x0}" y="52">t={time}s; rejected cells={folded}</text>')
        for row, values in enumerate((old, new, gradient(old, size, 2*extent/size),
                                     gradient(new, size, 2*extent/size))):
            y0 = 65+row*210
            out.append(f'<text x="{x0}" y="{y0}">{("old height", "prototype height", "old slope", "prototype slope")[row]}</text>')
            cell = 190/size
            for z in range(size):
                for x in range(size):
                    v = values[z*size+x]
                    index = round(128+127*max(-1, min(1, v/.6))) if row < 2 else round(128+127*min(1, v/.5))
                    out.append(f'<rect x="{x0+x*cell:.2f}" y="{y0+8+z*cell:.2f}" width="{cell+.03:.2f}" height="{cell+.03:.2f}" fill="{colors[index]}"/>')
        # Fixed world section x~0, across the travelling train rather than along its source.
        y0 = 980
        out.append(f'<path d="M{x0},{y0}h320" stroke="#526779"/>')
        for values, color in ((old, '#edba70'), (new, '#74d5ed')):
            section = [values[z*size+size//2] for z in range(size)]
            points = ' '.join(f'{x0+i*320/(size-1):.2f},{y0-v*75:.2f}' for i, v in enumerate(section))
            out.append(f'<polyline points="{points}" fill="none" stroke="{color}"/>')
        out.append(f'<text x="{x0}" y="1030">side: x~0, z +/-{extent}m; 75px/m</text>')
    out.append('</g></svg>')
    path.write_text('\n'.join(out))


def png(path, frames, size, extent):
    # Portable raster companion to the labelled SVG, requiring no graphics context.
    # Columns: 16/24/32 s. Rows: old height, new height, old slope, new slope.
    width, height = 3*size, 4*size+100
    pixels = bytearray(width*height*3)
    for col, (_, old, new, _) in enumerate(frames):
        arrays = old, new, gradient(old, size, 2*extent/size), gradient(new, size, 2*extent/size)
        for row, values in enumerate(arrays):
            for z in range(size):
                for x in range(size):
                    v = values[z*size+x]
                    v = max(-1, min(1, v/.6)) if row < 2 else min(1, v/.5)
                    rgb = (int(40+max(v, 0)*210), int(80+abs(v)*100),
                           int(120+max(-v, 0)*130))
                    i = ((row*size+z)*width+col*size+x)*3
                    pixels[i:i+3] = bytes(rgb)
        for values, color in ((old, (237, 186, 112)), (new, (116, 213, 237))):
            section = [values[z*size+size//2] for z in range(size)]
            previous = None
            for x, v in enumerate(section):
                y = round(4*size+50-v*75)
                if previous is not None:
                    for yy in range(max(4*size, min(previous, y)), min(height-1, max(previous, y))+1):
                        i = (yy*width+col*size+x)*3
                        pixels[i:i+3] = bytes(color)
                previous = y
    raw = b''.join(b'\0'+pixels[y*width*3:(y+1)*width*3] for y in range(height))
    def chunk(kind, data):
        return struct.pack('!I', len(data))+kind+data+struct.pack('!I', zlib.crc32(kind+data))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('!2I5B', width, height, 8, 2, 0, 0, 0))
                     +chunk(b'IDAT', zlib.compress(raw))+chunk(b'IEND', b''))


def review(args):
    args.output.mkdir(parents=True, exist_ok=True)
    size, extent = 128, 96.
    step = 2*extent/size
    points = [complex(-extent+(x+.5)*step, -extent+(z+.5)*step)
              for z in range(size) for x in range(size)]
    report = {'parameters': {'wavelength_m': 6, 'cp_m_s': CP, 'cg_m_s': CG,
                            'width_m': 24, 'source_duration_s': 12,
                            'grid_spacing_m': step, 'node_spacing_s': .4},
              'baseline': {}, 'prototype': {}}
    for name in ('straight', 'loop', 's-turn', 'crossing'):
        frames, measures = [], []
        for time in (16, 24, 32):
            old = baseline(args.baseline, name, time, points)
            h, psi, folded = field(name, time, size, extent)
            frames.append((time, [r[0] for r in old], h, folded))
            measures.append({'time_s': time, 'old_peak_m': max(abs(r[0]) for r in old),
                             'prototype_peak_m': max(map(abs, h)),
                             'rejected_fold_cells': folded,
                             'old_height_squared_m4': sum(r[0]**2 for r in old)*step**2,
                             'prototype_volume_m3': sum(h)*step**2})
        svg(args.output / f'{name}.svg', name, frames, size, extent)
        png(args.output / f'{name}.png', frames, size, extent)
        report['baseline'][name] = measures
    # Isolate seams: source station centres and halfway stations, tracking each patch's
    # positive-side envelope at t=16 s (station age approximately 10 s). RMS height over one wavelength
    # avoids mistaking a carrier zero for an absent wave.
    seam = {}
    for name in ('straight', 'loop', 's-turn'):
        values = []
        for s in (5.8, 6.0):
            c, n = source(name, s)
            t = 16
            radius = .9+1.6*(t-s)
            samples = baseline(args.baseline, name, t,
                               [c+n*(radius-3+6*j/120) for j in range(121)])
            values.append(math.sqrt(sum(r[0]**2 for r in samples)/len(samples)))
        seam[name] = {'station_rms_m': values[0], 'join_rms_m': values[1],
                      'join_to_station_ratio': values[1]/values[0] if values[0] else None}
    report['baseline']['station_join_probe'] = seam
    lifetime = []
    for age in (1.4, 8, 14, 17, 19, 20, 24):
        radius = .9+1.6*age
        rows = baseline(args.baseline, 'packet', age,
                        [complex(0, radius-4.5+9*j/360) for j in range(361)])
        lifetime.append({'age_s': age, 'peak_m': max(abs(r[0]) for r in rows),
                         'height_squared_cross_section_m3': sum(r[0]**2 for r in rows)*.025})
    report['baseline']['isolated_packet_lifetime'] = lifetime
    # Local material integration follows the entire spreading packet for 60 s, avoiding
    # camera/domain clipping. Compare both step/node refinements and actual reconstructed
    # Hamiltonians on a 384m FFT domain for a finite straight packet.
    audits = []
    for spacing, dq in ((.4, .5), (.2, .25)):
        front = Front('spreading', spacing)
        for time in (16, 46, 76):
            volume = potential = area_weight = 0.
            ratios = []
            # outward side of the circular arc (normal points inward)
            for a, b in zip(front.nodes, front.nodes[1:]):
                n = interpolate(a, b, .5)
                for j in range(round(24/dq)):
                    q = -12+(j+.5)*dq
                    _, h, _, ratio, jac = evaluate(n, q, time, -1)
                    cell = (b[0]-a[0])*dq
                    volume += h*jac*cell
                    potential += RHO*G/2*h*h*jac*cell
                    area_weight += jac*cell
                    ratios.append(ratio)
            audits.append({'node_spacing_s': spacing, 'dq_m': dq, 'time_s': time,
                           'potential_energy_j': potential, 'volume_m3': volume,
                           'area_m2': area_weight, 'min_area_ratio': min(ratios),
                           'centre_amplitude_m': AMPLITUDE / math.sqrt(evaluate(node('spreading', 6), 0, time, -1)[3])})
    report['prototype']['spreading_material_audit'] = audits
    audits = []
    for ngrid, spacing, dq in ((256, .4, .5), (512, .2, .25)):
        for time in (16, 31, 46, 61, 76):
            h, psi, folds = field('straight', time, ngrid, 192., spacing, dq)
            edge = max(abs(h[z*ngrid+x]) for z in range(ngrid) for x in (0, ngrid-1))
            edge = max(edge, max(map(abs, h[:ngrid]+h[-ngrid:])))
            audits.append({'grid_size': ngrid, 'node_spacing_s': spacing, 'dq_m': dq,
                           'time_s': time, 'hamiltonian_j': energy(h, psi, ngrid, 192.),
                           'edge_height_m': edge, 'folds': folds})
    report['prototype']['reconstructed_energy_audit'] = audits
    # Kinetic and potential crossing terms are included by auditing the summed fields.
    crossing = []
    for time in (16, 24, 32):
        a, p, _ = field('crossing', time, 128, sources=[1+0j])
        b, q, _ = field('crossing', time, 128, sources=[1j])
        h, psi = [x+y for x, y in zip(a, b)], [x+y for x, y in zip(p, q)]
        independent = energy(a, p, 128, 96.) + energy(b, q, 128, 96.)
        total = energy(h, psi, 128, 96.)
        crossing.append({'time_s': time, 'sum_field_energy_j': total,
                         'independent_energy_j': independent,
                         'interference_energy_j': total-independent,
                         'peak_sum_m': max(map(abs, h)),
                         'peak_isolated_m': max(map(abs, a)),
                         'opposite_phase_energy_j': energy([x-y for x, y in zip(a, b)],
                                                          [x-y for x, y in zip(p, q)], 128, 96.)})
    report['prototype']['crossing_energy_audit'] = crossing
    report['tolerances'] = {'energy_drift_fraction_over_60s': .01,
                            'subdivision_field_absolute_m': 1e-8,
                            'relative_mean_level': .001,
                            'accepted_fold_cells': 0, 'energy_refinement_fraction': .01}
    energy_audit = report['prototype']['reconstructed_energy_audit']
    refined = [m['hamiltonian_j'] for m in energy_audit if m['grid_size'] == 512]
    drift = max(abs(v/refined[0]-1) for v in refined)
    convergence = abs(energy_audit[0]['hamiltonian_j']/refined[0]-1)
    report['prototype']['energy_refinement_fraction'] = convergence
    report['prototype']['refined_60s_energy_drift_fraction'] = drift
    report['failed_physical_gates'] = []
    if any(m['rejected_fold_cells'] for name in ('loop', 's-turn')
           for m in report['baseline'][name]):
        report['failed_physical_gates'].append('connected inner fronts fold')
    if convergence >= .01:
        report['failed_physical_gates'].append('reconstructed energy not converged within 1%')
    if drift >= .01:
        report['failed_physical_gates'].append('reconstructed 60s energy drift exceeds 1%')
    if any(abs(m['volume_m3'])/(m['area_m2']*AMPLITUDE) >= .001
           for m in report['prototype']['spreading_material_audit']):
        report['failed_physical_gates'].append('curved reconstruction fails mean-level tolerance')
    report['decision'] = ('Reject this connected-strip reconstruction at W1. Do not integrate '
                          'it into the player; investigate a linear dispersive heightfield '
                          'solver before W2.' if report['failed_physical_gates'] else
                          'Measured gates passed; further source/propagation work remains W2.')
    (args.output/'measurements.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--output', type=Path, default=Path('build/wake-w1'))
    args = parser.parse_args()
    check()
    if args.check:
        if args.baseline is not None:
            rows = baseline(args.baseline, 'packet', 20, [0j, 32j, 35j])
            assert all(row == [0., 0., 0., 0.] for row in rows)
    else:
        args.baseline = args.baseline or Path('build/wake_baseline')
        review(args)


if __name__ == '__main__':
    main()
