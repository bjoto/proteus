#!/usr/bin/env python3
"""W1 fallback: exact linear deep-water evolution on an isolated periodic grid.

Standard library only. No runtime imports this diagnostic. Source strength is prescribed,
not calibrated hull work. Fourier coefficients use the normalized Fourier-series convention.
"""
import argparse
import cmath
import json
import math
from pathlib import Path

from wake_front_review import G, RHO, K, OMEGA, source, fft, energy, baseline, png, svg

SCALE = .3 * G / OMEGA  # 0.3 m/s prescribed height-rate; potential forcing in m²/s²
SIGMA_T, SIGMA_N = 2.5, 5.


def transform(values, size, inverse=False):
    """2D FFT; inverse includes normalization, with a real-field residual check."""
    if inverse:
        values = [v.conjugate() for v in values]
    rows = [fft(values[z*size:(z+1)*size]) for z in range(size)]
    result = [0j] * (size*size)
    for x in range(size):
        column = fft([row[x] for row in rows])
        for z, v in enumerate(column):
            result[z*size+x] = v.conjugate()/(size*size) if inverse else v
    return result


class Grid:
    def __init__(self, size=256, length=384.):
        if size < 8 or size & (size-1):
            raise ValueError('grid size must be a power of two >= 8')
        self.size, self.length = size, length
        self.modes = []
        for z in range(size):
            kz = 2*math.pi/length * (z if z < size//2 else z-size)
            for x in range(size):
                kx = 2*math.pi/length * (x if x < size//2 else x-size)
                k = math.hypot(kx, kz)
                self.modes.append((kx, kz, k, math.sqrt(G*k)))

    def evolve(self, h, p, dt):
        a, b = [], []
        for eta, psi, (_, _, k, omega) in zip(h, p, self.modes):
            if k == 0:
                a.append(eta)
                b.append(psi-G*eta*dt)
            else:
                c, s = math.cos(omega*dt), math.sin(omega*dt)
                a.append(c*eta+k/omega*s*psi)
                b.append(c*psi-G/omega*s*eta)
        return a, b

    def energy(self, h, p):
        return RHO/2*self.length**2*sum(G*abs(a)**2+k*abs(b)**2
                                      for a, b, (_, _, k, _) in zip(h, p, self.modes))

    def field(self, coefficients):
        values = transform([v*self.size**2 for v in coefficients], self.size, True)
        assert max(abs(v.imag) for v in values) < 1e-9
        return [v.real for v in values]

    def emit(self, name, dt=.2, rotations=None):
        """Midpoint potential impulses on the continuous W1 stern paths, 0 <= s < 12.

        Each balanced Gaussian-carrier source is a transverse second derivative;
        its k=0 coefficient is exactly zero. Analytic Fourier deposition avoids splats.
        Nyquist lines are omitted to preserve Hermitian symmetry for rotated sources.
        """
        count = round(12/dt)
        if count < 1 or abs(count*dt-12) > 1e-9:
            raise ValueError('emission step must divide 12 s')
        rotations = rotations if rotations is not None else ([1, 1j] if name == 'crossing' else [1])
        stations = []
        for rotation in rotations:
            for j in range(count):
                s = (j+.5)*dt
                c, n = source(name, s)
                c, n = c*rotation, n*rotation
                c += complex(self.length/2, self.length/2)
                weight = -SCALE*dt*math.sin(math.pi*s/12)**2
                stations.append((s, c, n, weight))
        stations.sort(key=lambda station: station[0])
        h, p = [], []
        # Audit exact kick work including the existing field's cross term. Work may
        # be signed: this is prescribed forcing, not a positive engine budget.
        work = [0.] * len(stations)
        for i, (kx, kz, k, omega) in enumerate(self.modes):
            eta = psi = 0j
            if k and i % self.size != self.size//2 and i//self.size != self.size//2:
                for j, (s, c, n, weight) in enumerate(stations):
                    kn, kt = kx*n.real+kz*n.imag, kx*n.imag-kz*n.real
                    envelope = math.exp(-.5*(SIGMA_T*kt)**2)
                    envelope *= (math.exp(-.5*(SIGMA_N*(kn-K))**2)
                                 + math.exp(-.5*(SIGMA_N*(kn+K))**2))
                    if envelope < 1e-16:
                        continue
                    shape = math.pi*SIGMA_T*SIGMA_N/self.length**2 * (kn/K)**2*envelope
                    impulse = weight*shape*cmath.exp(-1j*(kx*c.real+kz*c.imag))
                    # Contributions are accumulated at t=12, so evaluate existing psi
                    # at this kick's time using the inverse homogeneous propagator.
                    age = 12-s
                    cs, sn = math.cos(omega*age), math.sin(omega*age)
                    old_psi = cs*psi+G/omega*sn*eta
                    work[j] += RHO/2*self.length**2*k*(2*(old_psi.conjugate()*impulse).real
                                                                   + abs(impulse)**2)
                    eta += k/omega*sn*impulse
                    psi += cs*impulse
            h.append(eta)
            p.append(psi)
        return h, p, work


def travel_probe():
    # A one-direction finite-band analytic signal; centroid uses envelope intensity.
    # Long periodic box keeps the packet far from its wrap at both samples.
    size, length, sigma, centre = 512, 384., .18, -40.
    ks = [2*math.pi*j/length for j in range(size)]
    amplitudes = [math.exp(-.5*((k-K)/sigma)**2) if 0 < j < size//2 else 0.
                  for j, k in enumerate(ks)]
    weights = [v*v for v in amplitudes]
    expected = sum(w*.5*math.sqrt(G/k) for k, w in zip(ks[1:], weights[1:]))/sum(weights)
    moments = []
    for t in (0., 60.):
        coefficients = [a*cmath.exp(-1j*(k*(centre+length/2)+math.sqrt(G*k)*t))
                        for k, a in zip(ks, amplitudes)]
        signal = [v.conjugate()/size for v in fft([v.conjugate() for v in coefficients])]
        density = [abs(v)**2 for v in signal]
        xs = [-length/2+j*length/size for j in range(size)]
        mean = sum(x*w for x, w in zip(xs, density))/sum(density)
        width = math.sqrt(sum((x-mean)**2*w for x, w in zip(xs, density))/sum(density))
        moments.append({'time_s': t, 'centroid_m': mean, 'rms_width_m': width,
                        'peak_envelope': max(map(abs, signal))})
    observed = (moments[1]['centroid_m']-moments[0]['centroid_m'])/60
    return {'carrier_phase_speed_m_s': OMEGA/K, 'carrier_group_speed_m_s': OMEGA/K/2,
            'band_weighted_group_speed_m_s': expected, 'measured_centroid_speed_m_s': observed,
            'group_speed_error_m_s': abs(observed-expected), 'samples': moments}


def check():
    probe = travel_probe()
    assert probe['group_speed_error_m_s'] < 1e-9
    assert probe['samples'][1]['rms_width_m'] > probe['samples'][0]['rms_width_m']
    grid = Grid(16, 48.)
    h, p = [0j]*256, [0j]*256
    h[1] = h[15] = .15
    omega = grid.modes[1][3]
    a, b = grid.evolve(h, p, math.pi/(2*omega))
    assert max(map(abs, a)) < 1e-14
    assert abs(grid.energy(a, b)/grid.energy(h, p)-1) < 1e-12
    assert abs(energy(grid.field(a), grid.field(b), 16, 24.)/grid.energy(a, b)-1) < 1e-12
    # A traveling Fourier mode checks phase sign and exact crest travel independently.
    traveling = [0j]*256
    traveling[1], traveling[15] = -1j*G/omega*.15, 1j*G/omega*.15
    advanced, _ = grid.evolve(h, traveling, 2.3)
    expected = [.3*math.cos(2*math.pi*x/16-omega*2.3) for z in range(16) for x in range(16)]
    assert max(abs(x-y) for x, y in zip(grid.field(advanced), expected)) < 1e-12
    # Uniform advection multiplies both fields by the same translation phase.
    shifted_h = [v*cmath.exp(-1j*(kx*2-kz)*7) for v, (kx, kz, _, _) in zip(h, grid.modes)]
    shifted_p = [v*cmath.exp(-1j*(kx*2-kz)*7) for v, (kx, kz, _, _) in zip(traveling, grid.modes)]
    assert abs(grid.energy(shifted_h, shifted_p)/grid.energy(h, traveling)-1) < 1e-12
    c, d = grid.evolve(a, b, -math.pi/(2*omega))
    assert max(abs(x-y) for x, y in zip(h, c)) < 1e-14
    # Independent inverse FFT and finite differences check the evolution equations.
    h, p, work = grid.emit('loop', .4)
    assert h[0] == p[0] == 0
    assert abs(sum(work)/grid.energy(h, p)-1) < 1e-12
    a, b = grid.evolve(h, p, 60.)
    assert abs(grid.energy(a, b)/grid.energy(h, p)-1) < 1e-12
    eps = 1e-4
    before = grid.evolve(h, p, -eps)
    after = grid.evolve(h, p, eps)
    assert max(abs((y-x)/(2*eps)-k*v) for x, y, v, (_, _, k, _) in
               zip(before[0], after[0], p, grid.modes)) < 1e-9
    assert max(abs((y-x)/(2*eps)+G*v) for x, y, v in
               zip(before[1], after[1], h)) < 1e-9
    assert abs(energy(grid.field(a), grid.field(b), 16, 24.)/grid.energy(a, b)-1) < 1e-12
    x, y, _ = grid.emit('crossing', .4)
    r, s, _ = grid.emit('straight', .4, [1])
    u, v, _ = grid.emit('straight', .4, [1j])
    assert max(abs(a-b-c) for a, b, c in zip(x+y, r+s, u+v)) < 1e-12
    zero = [a-a for a in x]
    assert grid.energy(zero, zero) == 0
    # Exact propagator composes without a numerical integration step-size dependency.
    c, d = grid.evolve(*grid.evolve(h, p, 30.), 30.)
    assert max(abs(x-y) for x, y in zip(a+b, c+d)) < 1e-12
    print('Heightfield numerical checks passed; gameplay/source acceptance remains separate.')


def review(args):
    args.output.mkdir(parents=True, exist_ok=True)
    report = {'parameters': {'length_m': 384, 'source_duration_s': 12,
                            'sigma_t_m': SIGMA_T, 'sigma_n_m': SIGMA_N,
                            'wavelength_m': 6, 'density_kg_m3': RHO,
                            'boundary': 'periodic; no damping, shores or retirement',
                            'source': 'prescribed balanced potential impulses; uncalibrated'},
              'travel_probe': travel_probe(), 'paths': {}, 'failed_gates': []}
    for name in ('straight', 'loop', 's-turn', 'crossing'):
        print(f'Reviewing {name}', flush=True)
        runs = []
        states = []
        # Isolate source-time refinement from grid refinement, then halve both.
        for size, dt in ((256, .4), (256, .2), (256, .1), (512, .1)):
            print(f'  {size} grid, source step {dt}s', flush=True)
            grid = Grid(size)
            h, p, work = grid.emit(name, dt)
            e0 = grid.energy(h, p)
            audit = []
            for t in (16, 31, 46, 61, 76):
                a, b = grid.evolve(h, p, t-12)
                height, potential = grid.field(a), grid.field(b)
                reconstructed = energy(height, potential, size, grid.length/2)
                edge = max(abs(height[z*size+x]) for z in range(size) for x in (0, size-1))
                edge = max(edge, max(map(abs, height[:size]+height[-size:])))
                audit.append({'time_s': t, 'energy_j': reconstructed,
                              'peak_m': max(map(abs, height)), 'edge_peak_m': edge,
                              'mean_m': sum(height)/(size*size)})
            runs.append({'size': size, 'dt_s': dt, 'energy_at_source_off_j': e0,
                         'injected_work_j': sum(work), 'negative_kick_work_j': sum(min(0, w) for w in work),
                         'reconstruction': audit,
                         'drift_fraction': max(abs(m['energy_j']/e0-1) for m in audit),
                         'work_closure_fraction': abs(sum(work)/e0-1)})
            states.append((grid, h, p))
        # Compare actual co-located heights, not just a scalar energy counter.
        coarse_grid, ch, cp = states[-2]
        fine_grid, fh, fp = states[-1]
        coarse = coarse_grid.field(coarse_grid.evolve(ch, cp, 4)[0])
        fine_field = fine_grid.field(fine_grid.evolve(fh, fp, 4)[0])
        sampled = [fine_field[2*z*512+2*x] for z in range(256) for x in range(256)]
        field_error = math.sqrt(sum((a-b)**2 for a, b in zip(coarse, sampled))
                                /sum(v*v for v in sampled))
        fine = runs[-1]['energy_at_source_off_j']
        convergence = abs(runs[-2]['energy_at_source_off_j']/fine-1)
        temporal = abs(runs[1]['energy_at_source_off_j']/runs[2]['energy_at_source_off_j']-1)
        mean = max(abs(m['mean_m'])/.3 for r in runs for m in r['reconstruction'])
        for label, value, tolerance in (('grid energy refinement', convergence, .01),
                                         ('source time refinement .2/.1', temporal, .01),
                                         ('60s energy drift', max(r['drift_fraction'] for r in runs), .01),
                                         ('relative mean level', mean, .001),
                                         ('height L2 grid refinement', field_error, .01)):
            if value >= tolerance:
                report['failed_gates'].append(f'{name}: {label}')
        report['paths'][name] = {'runs': runs, 'grid_refinement_fraction': convergence,
                                 'source_refinement_fraction': temporal,
                                 'height_l2_grid_refinement_fraction': field_error}
        grid, h, p = states[-1]
        frames = []
        # Co-located 1.5 m display samples cropped to the same +/-96 m W1 view.
        points = [complex(-96+x*1.5, -96+z*1.5) for z in range(128) for x in range(128)]
        for t in (16, 24, 32):
            values = grid.field(grid.evolve(h, p, t-12)[0])
            shown = [values[(128+z*2)*512+128+x*2] for z in range(128) for x in range(128)]
            old = [r[0] for r in baseline(args.baseline, name, t, points)]
            frames.append((t, old, shown, 0))
        svg(args.output/f'{name}.svg', name, frames, 128, 96.)
        # Reuse the W1 plotting layout, correcting its strip-specific labels.
        path = args.output/f'{name}.svg'
        path.write_text(path.read_text().replace('old / connected strip', 'old / linear heightfield')
                        .replace('rejected cells=0', 'periodic grid; no omitted cells'))
        png(args.output/f'{name}.png', frames, 128, 96.)
        if name == 'crossing':
            a, b, _ = grid.emit('straight', .1, [1])
            c, d, _ = grid.emit('straight', .1, [1j])
            report['crossing'] = {'combined_energy_j': fine,
                                  'independent_energy_j': grid.energy(a, b)+grid.energy(c, d),
                                  'interference_energy_j': fine-grid.energy(a, b)-grid.energy(c, d),
                                  'opposite_phase_energy_j': grid.energy([x-y for x, y in zip(a, c)],
                                                                        [x-y for x, y in zip(b, d)])}
    report['tolerances'] = {'energy_drift_fraction': .01, 'energy_refinement_fraction': .01,
                            'height_l2_refinement_fraction': .01, 'relative_mean_level': .001}
    report['decision'] = ('Reject at the measured gates.' if report['failed_gates'] else
                          'Free-propagation gates pass for this periodic diagnostic; revise W2. '
                          'No player integration or moving-source calibration is accepted.')
    (args.output/'measurements.json').write_text(json.dumps(report, indent=2)+'\n')
    print(report['decision'], flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--baseline', type=Path, default=Path('build/wake_baseline'))
    parser.add_argument('--output', type=Path, default=Path('build/wake-heightfield'))
    args = parser.parse_args()
    check()
    if not args.check:
        review(args)


if __name__ == '__main__':
    main()
