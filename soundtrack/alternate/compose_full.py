#!/usr/bin/env python3
"""Expand approved proof v03 into the first full composition; never overwrite a score."""

import argparse
from copy import deepcopy
import json
from pathlib import Path
import re

from render import validate

HERE = Path(__file__).resolve().parent


def make_project():
    approved = json.loads((HERE / 'proof-v03.json').read_text())
    tracks = {t['name']: {**deepcopy(t), 'notes': []} for t in approved['tracks']}
    harmony = []

    def add(name, beat, length, pitch, velocity):
        tracks[name]['notes'].append([round(beat, 4), round(length, 4), pitch, velocity])

    def chord(name, beat, length, root, velocity, voices=3):
        intervals = ([0, 7, 12] if name == 'low strings' else [7, 12, 19, 25])
        for i, interval in enumerate(intervals[:voices]):
            add(name, beat, length, root + interval, velocity - i * 3)

    def mark(bar, root, role):
        names = {34: 'Bb5', 36: 'C5', 39: 'Eb5', 40: 'E5', 41: 'F5'}
        harmony.append(dict(beat=bar * 4, seconds=bar * 2, chord=names[root], role=role))

    def motif(bar, root, velocity, stretch=1, octave=0, fractured=False, fragment=False):
        phrase = [(0, 2.6, 12, 0), (4, .85, 13, -8),
                  (6, 1.5, 6 if fractured else 7, -3)]
        for offset, length, interval, gain in phrase[:2 if fragment else 3]:
            add('three-note signal', bar * 4 + offset * stretch,
                length * stretch, root + interval + octave, velocity + gain)

    def pulse(bar, root, velocity, density=1):
        patterns = {
            0: [(0, .65, 0), (2.5, .42, -15)],
            1: [(0, .60, 0), (1.5, .36, -20), (3, .55, -9)],
            2: [(0, .60, 0), (.75, .28, -26), (1.5, .32, -14),
                (2.5, .32, -6), (3.25, .4, -18)],
        }
        for offset, length, gain in patterns[min(2, density)]:
            add('growling pulse', bar * 4 + offset, length, root, velocity + gain)
        if density == 3:
            add('growling pulse', bar * 4 + 3.75, .16,
                root + (-1 if bar % 4 == 3 else 1), velocity - 28)

    def drums(bar, velocity, density=1, broken=False):
        beat = bar * 4
        add('percussion', beat + (1.5 if broken and bar % 2 else 0), .15, 36, velocity)
        if density == 0:
            if bar % 2:
                add('percussion', beat + 2.5, .2, 41, velocity - 24)
            return
        add('percussion', beat + 2.75, .15, 36, velocity - 13)
        add('percussion', beat + 2, .17, 38, velocity - 16)
        if bar % 2 == 0:
            add('percussion', beat + 1.5, .14, 42, 37)
        if density == 2:
            add('percussion', beat + 3.5, .18, 43, velocity - 24)
            add('percussion', beat + 3.75, .18, 41, velocity - 17)

    # 0:00–0:24, bars 0–11. The approved signal emerges from a distant pedal.
    for bar, root, velocity in [(0, 40, 64), (4, 40, 70), (8, 36, 61)]:
        mark(bar, root, 'Opening pedal and its first descent.')
        add('deep drone', bar * 4, 10, root - 12, velocity)
    chord('low choir', 14, 9, 40, 38, voices=2)
    chord('low strings', 32, 9, 36, 38, voices=2)
    motif(2, 40, 61)
    motif(8, 36, 60)

    # 0:24–0:56, bars 12–27. A long, slow chord cycle introduces the pulse.
    for block, root in enumerate([40, 36, 41, 40]):
        bar = 12 + block * 4
        mark(bar, root, 'Awakening: eight seconds per root.')
        add('deep drone', bar * 4, 10, root - 12, 56 + block * 3)
        chord('low strings', bar * 4 + 4, 8, root, 45 + block * 3, voices=2)
        if block:
            motif(bar, root, 65 + block * 3)
        if block >= 2:
            chord('low choir', bar * 4 + 6, 7, root, 43 + block * 2, voices=2)
        for i in range(4):
            b = bar + i
            pulse(b, root, 68 + block * 5 + i, density=0 if block < 2 else 1)
            drums(b, 67 + block * 5 + i, density=0 if block < 3 else 1)

    # 0:56–1:28, bars 28–43. First peak: low register and a restrained choir.
    for block, root in enumerate([40, 36, 41, 40] * 2):
        bar = 28 + block * 2
        mark(bar, root, 'First weight: the accepted power-chord cycle.')
        add('deep drone', bar * 4, 5.5, root - 12, 72 + block % 4 * 2)
        chord('low strings', bar * 4, 6, root, 60 + block // 4 * 4)
        if block >= 4:
            chord('low choir', bar * 4, 5.5, root, 52, voices=2)
        if block % 2 == 0:
            motif(bar, root, 86 + block)
        for i in range(2):
            pulse(bar + i, root, 92 + block // 4 * 4, density=2)
            drums(bar + i, 92 + block // 4 * 4,
                  density=2 if block == 7 and i == 1 else 1)

    # 1:28–1:56, bars 44–57. Break the pulse; lower the motif's fifth to a tritone.
    for block, root in enumerate([40, 41, 39, 34, 40, 36, 41]):
        bar = 44 + block * 2
        mark(bar, root, 'Fracture: chromatic roots and interrupted rhythm.')
        if block != 3:
            add('deep drone', bar * 4, 4, root - 12, 67 + block % 3 * 4)
        chord('low strings', bar * 4, 3 if block % 2 else 5, root, 58)
        if block % 2 == 0:
            motif(bar, root, 81, fractured=True, fragment=block == 6)
        for i in range(2):
            if (block + i) % 3 != 1:
                pulse(bar + i, root, 91, density=2 if i == 0 else 0)
            drums(bar + i, 91 if i == 0 else 73, density=i, broken=True)
    # End the fracture on a single low impact, then let the room empty.
    for track in tracks.values():
        track['notes'] = [n for n in track['notes'] if n[0] < 228]
        for note in track['notes']:
            if note[0] + note[1] > 228:
                note[1] = round(228 - note[0], 4)
    add('percussion', 228, .2, 36, 99)
    add('percussion', 228, .3, 41, 89)
    add('growling pulse', 228, .7, 34, 89)

    # 1:56–2:28, bars 58–73. The quiet centre carries its own two slow statements.
    for bar, root in [(58, 36), (66, 40)]:
        mark(bar, root, 'Suspended world: hold space around the slowed signal.')
        add('deep drone', bar * 4 + 1, 18, root - 12, 31)
        chord('low strings', bar * 4 + 10, 10, root, 39, voices=2)
        chord('low choir', bar * 4 + 19, 7, root, 37, voices=2)
        motif(bar + 2, root, 64, stretch=1.6)
    add('three-note signal', 258, 3.4, 43, 46)
    add('growling pulse', 294, .6, 40, 49)

    # 2:28–2:56, bars 74–87. Return to the approved harmonic path and build density.
    for block, (root, bars) in enumerate([(40, 4), (36, 4), (41, 4), (40, 2)]):
        bar = 74 + block * 4
        mark(bar, root, 'Gathering: accumulate weight, then leave a breath.')
        add('deep drone', bar * 4, bars * 4 - 4, root - 12, 48 + block * 9)
        chord('low strings', bar * 4 + 1, bars * 4 - 3, root,
              48 + block * 7, voices=2 if block < 2 else 3)
        motif(bar, root, 68 + block * 8, fragment=block == 3)
        if block >= 2:
            chord('low choir', bar * 4 + 2, bars * 4 - 4, root, 52, voices=2)
        for i in range(bars):
            pulse(bar + i, root, 74 + block * 8 + i * 2, density=min(2, block))
            drums(bar + i, 72 + block * 8 + i * 2, density=0 if block == 0 else 1)
    for i in range(4):
        add('percussion', 348 + i * .375, .12, 41 if i < 2 else 43, 68 + i * 8)
    for track in tracks.values():
        track['notes'] = [n for n in track['notes'] if n[0] < 350]
        for note in track['notes']:
            if note[0] + note[1] > 350:
                note[1] = round(350 - note[0], 4)

    # 2:56–3:36, bars 88–107. The largest peak stays dark; widen only the later half.
    for block, root in enumerate([40, 36, 41, 40] * 2 + [40, 36]):
        bar = 88 + block * 2
        mark(bar, root, 'Full return: weight and range rather than a major-key lift.')
        add('deep drone', bar * 4, 5.2, root - 12, 98 if block < 4 else 107)
        chord('low strings', bar * 4, 5.8, root, 85 if block < 4 else 93)
        chord('low choir', bar * 4, 5.5, root, 71 if block < 4 else 80,
              voices=3 if block < 4 else 4)
        if block % 2 == 0:
            motif(bar, root, 109 if block < 4 else 117,
                  octave=12 if block in (4, 6) else 0)
        for i in range(2):
            # The last bar moves to F to set up the final semitone fall into E.
            r = 41 if bar + i == 107 else root
            if r != root:
                mark(bar + i, r, 'Final upper-neighbour approach.')
            pulse(bar + i, r, 111 if block < 4 else 120, density=3)
            drums(bar + i, 111 if block < 4 else 122,
                  density=2 if i == 1 and block % 2 else 1)

    # 3:36–3:52: decisive impact, then the opening signal and bare fifth recede.
    mark(108, 40, 'Afterglow: settle onto the home pedal without a major third.')
    add('percussion', 432, .2, 36, 113)
    add('percussion', 432, .3, 41, 103)
    add('deep drone', 432, 6, 28, 80)
    chord('low strings', 432, 7, 40, 71)
    chord('low choir', 432, 5, 40, 61)
    add('three-note signal', 432, 3.6, 52, 90)
    add('deep drone', 448, 10, 28, 46)
    motif(112, 40, 59)
    chord('low strings', 452, 8, 40, 39, voices=2)
    add('three-note signal', 460, 2, 52, 45)

    sections = [
        (0, 'The signal', 'Distant drone, open fifths, and the sparse three-note motif.'),
        (48, 'Awakening', 'Slow chord changes; the pulse gradually takes shape.'),
        (112, 'First weight', 'First complete pressure cycle, kept low and restrained.'),
        (176, 'Fracture', 'Interrupted pulse, chromatic roots, and the altered motif.'),
        (232, 'Suspended world', 'A spacious centre with two slow signal statements.'),
        (296, 'The gathering', 'The familiar chords return; density and weight rise.'),
        (350, 'Held breath', 'Parts release before the final entrance.'),
        (352, 'Full return', 'The largest cycle begins with all six voices.'),
        (384, 'Widening', 'The upper signal register and four-voice choir enter.'),
        (432, 'Afterglow', 'A final impact, then the opening signal retreats.'),
        (464, 'Release', 'Eight seconds for the instruments and hall to decay.'),
    ]
    project = {k: deepcopy(v) for k, v in approved.items()
               if k not in ('tracks', 'sections', 'harmony', 'title', 'beats', 'tail_seconds')}
    project.update(title='Proteus — Below the surface — full sketch v01', beats=464,
                   tail_seconds=8, tracks=list(tracks.values()), harmony=harmony,
                   sections=[dict(beat=b, seconds=b / 2, name=n, description=d)
                             for b, n, d in sections])
    for track in project['tracks']:
        track['notes'].sort()
    validate(project)
    return project


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=HERE / 'full-v01.json')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('choose a new --output path; existing scores are preserved')
    project = make_project()
    text = json.dumps(project, indent=2, ensure_ascii=False)
    text = re.sub(r'\[\n\s+([\d.]+),\n\s+([\d.]+),\n\s+(\d+),\n\s+(\d+)\n\s+\]',
                  r'[\1, \2, \3, \4]', text)
    args.output.write_text(text + '\n')
    print(f'{args.output}: {sum(len(t["notes"]) for t in project["tracks"])} notes, 4:00')


if __name__ == '__main__':
    main()
