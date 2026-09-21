#!/usr/bin/env python3
"""Write the original 36-bar proof score. Render its JSON with render.py."""

import argparse
from copy import deepcopy
import json
from pathlib import Path
import re

from render import validate

HERE = Path(__file__).resolve().parent

# Beat, duration, semitones above E4. A rising fifth and minor seventh are the
# identifying gesture; the second half answers with a descending stepwise line.
MAIN = [(0, .65, 0), (.75, .20, 7), (1, .7, 10), (2, 1.8, 12),
        (4, 1.4, 15), (5.5, .4, 14), (6, .85, 12), (7, .75, 10),
        (8, 1.7, 7), (10, .75, 10), (11, 2.7, 12), (14, .7, 14), (15, .8, 7)]
# A more flowing, lower line: long notes and stepwise answers to MAIN's leaps.
LYRICAL = [(0, 1.4, 0), (1.5, .4, 3), (2, .85, 7), (3, .8, 5),
           (4, 2.4, 3), (6.5, 1.3, 0), (8, .85, 2), (9, .85, 0),
           (10, .85, -2), (11, .85, 2), (12, 1.7, -1), (14, .85, 2), (15, .8, -5)]

# Voicings leave space below the lead and avoid stacking every instrument at root.
HARMONY = {
    'Em': (40, [55, 59, 64, 67]), 'C': (36, [55, 60, 64, 67]),
    'G': (43, [55, 59, 62, 67]), 'D': (38, [54, 57, 62, 66]),
    'Am': (33, [52, 57, 60, 64]), 'B': (35, [54, 59, 63, 69]),
    'F': (41, [53, 57, 60, 65]), 'E': (40, [56, 59, 64, 68]),
    'A': (33, [52, 57, 61, 64]),
}


def make_project():
    initial = json.loads((HERE / 'palette-v01.json').read_text())
    dark = json.loads((HERE / 'palette-v02.json').read_text())
    tracks = {}
    for original, key in zip(initial['tracks'], ['lead', 'strings', 'bass', 'drums', 'choir']):
        tracks[key] = {**deepcopy(original), 'name': key, 'notes': []}
    for original, key in zip(dark['tracks'][:2], ['drone', 'pulse']):
        tracks[key] = {**deepcopy(original), 'name': key, 'notes': []}
    # Preserve the accepted synth patches; set their balance in this arrangement.
    for key, volume, pan in [('lead', 112, 66), ('strings', 82, 43), ('bass', 94, 64),
                             ('drums', 108, 64), ('choir', 82, 75), ('drone', 94, 64),
                             ('pulse', 96, 64)]:
        tracks[key].update(volume=volume, pan=pan)
    for key, program, channel, volume, pan in [
            ('lyrical flute', 73, 6, 96, 76), ('celesta', 8, 7, 84, 48),
            ('brass', 61, 8, 88, 55), ('pizzicato', 45, 10, 82, 86)]:
        tracks[key] = dict(name=key, program=program, channel=channel,
                           volume=volume, pan=pan, notes=[])

    def add(key, beat, length, pitch, velocity):
        tracks[key]['notes'].append([round(beat, 4), round(length, 4), pitch, velocity])

    def phrase(key, bar, melody, velocity, octave=0, major=False):
        for i, (offset, length, interval) in enumerate(melody):
            # Parallel-major transformation retains the contour and rhythm.
            if major and interval % 12 in (3, 10):
                interval += 1
            add(key, bar * 4 + offset, length, 64 + octave + interval,
                max(1, min(127, velocity + [0, -8, -3, 5][i % 4])))

    def chord(key, bar, name, velocity, duration=3.75, octave=0):
        for i, pitch in enumerate(HARMONY[name][1]):
            add(key, bar * 4, duration, pitch + octave, velocity - i * 2)

    def arp(bar, name, velocity, dense=False):
        pitches = HARMONY[name][1]
        for i in range(8 if dense else 4):
            add('pizzicato', bar * 4 + i * (.5 if dense else 1), .30,
                pitches[[0, 2, 1, 3, 2, 1, 3, 2][i]] + 12, velocity + (5 if i % 4 == 0 else -5))

    def rhythm(bar, intensity, final=False):
        start = bar * 4
        for offset in ([0, 1.5, 2, 3.5] if final else [0, 2.5]):
            add('drums', start + offset, .12, 36, intensity + (4 if offset == 0 else -7))
        for offset in ([1, 3] if final else [2]):
            add('drums', start + offset, .12, 38, intensity - 3)
        for i in range(8):
            add('drums', start + i * .5, .10, 42, intensity - (23 if i % 2 == 0 else 43))

    def pulse(bar, name, velocity, busy=False):
        root = HARMONY[name][0]
        for offset, length, interval in [(0, .55, 0), (.75, .3, 0), (1.5, .3, 7),
                                          (2.5, .3, 0), (3, .6, 0)]:
            add('pulse', bar * 4 + offset, length, root + interval,
                velocity if offset == 0 else velocity - 13)
        if busy:
            add('pulse', bar * 4 + 3.75, .15, root + 12, velocity - 20)

    # 0:00–0:08: the signal. A pedal and bell fragment plant the main gesture.
    add('drone', 0, 12, 28, 77)
    for pitch in [52, 59]:
        add('choir', 4, 8, pitch, 42)
    for beat, pitch, velocity in [(4, 76, 61), (7, 83, 49), (10, 86, 52), (12, 88, 60)]:
        add('celesta', beat, 1.5, pitch, velocity)

    # 0:08–0:24: two quiet statements. MAIN is fragile, then LYRICAL answers it.
    phrase('lyrical flute', 4, MAIN, 68)
    phrase('lyrical flute', 8, LYRICAL, 73)
    quiet_chords = ['Em', 'C', 'G', 'D', 'Em', 'C', 'Am', 'B']
    for i, name in enumerate(quiet_chords):
        bar = 4 + i
        chord('strings', bar, name, 43 + i % 3 * 3)
        add('bass', bar * 4, 2.8, HARMONY[name][0], 46)
        for offset, index in [(0, 0), (1.5, 2), (3, 1)]:
            add('celesta', bar * 4 + offset, .8, HARMONY[name][1][index] + 12, 44)

    # 0:24–0:40: pulse returns; the semitone threat gives way to a dominant pedal.
    build_chords = ['Em', 'Em', 'F', 'F', 'C', 'Am', 'B', 'B']
    for i, name in enumerate(build_chords):
        bar = 12 + i
        pulse(bar, name, 77 + i * 4, busy=i >= 4)
        chord('strings', bar, name, 54 + i * 4)
        arp(bar, name, 53 + i * 3, dense=i >= 4)
        if i % 2 == 0:
            add('drone', bar * 4, 5, HARMONY[name][0] - 12, 64 + i * 2)
        if i < 2:
            add('drums', bar * 4, .12, 36, 74 + i * 5)
        else:
            rhythm(bar, 77 + i * 3)
    # Fragments climb in register without delivering the full heroic phrase yet.
    for bar, pitches in [(16, [64, 71, 74]), (17, [69, 76, 79]), (18, [66, 71, 75])]:
        for offset, pitch in zip([0, 1.5, 3], pitches):
            add('brass', bar * 4 + offset, .7, pitch - 12, 77 + (bar - 16) * 5)
    for i in range(8):
        add('drums', 76 + i * .375, .10, 38, 62 + i * 7)
    # Half a second of anticipation: release most parts early, leaving their tails.
    for track in tracks.values():
        track['notes'] = [n for n in track['notes'] if not 79 <= n[0] < 80]
        for note in track['notes']:
            if note[0] < 79 < note[0] + note[1]:
                note[1] = round(79 - note[0], 4)

    # 0:40–1:04: three statements, the final one in parallel major with full choir.
    final_chords = ['Em', 'C', 'G', 'B', 'Em', 'C', 'Am', 'B', 'E', 'A', 'E', 'B']
    for section, bar in enumerate([20, 24, 28]):
        phrase('lead', bar, MAIN, 105 + section * 4, major=section == 2)
        phrase('lyrical flute', bar, LYRICAL, 73 + section * 4, major=section == 2)
    for i, name in enumerate(final_chords):
        bar = 20 + i
        root, pitches = HARMONY[name]
        chord('strings', bar, name, 85 + (6 if i >= 8 else 0))
        arp(bar, name, 68 + (6 if i >= 8 else 0), dense=True)
        rhythm(bar, 106 if i >= 8 else 100, final=True)
        add('drums', bar * 4, 1.0, 49, 93 if i in [0, 4, 8] else 47)
        for j, offset in enumerate([0, .75, 1.5, 2, 2.75, 3.5]):
            add('bass', bar * 4 + offset, .42, root + (7 if j == 4 else 0), 96 if j % 3 == 0 else 82)
        # The dark pulse becomes a quieter moving layer underneath the theme.
        pulse(bar, name, 70 if i < 8 else 78)
        for pitch in pitches[:3]:
            add('brass', bar * 4, 1.35, pitch, 88 if i < 8 else 98)
            add('brass', bar * 4 + 2.5, 1.2, pitch, 78 if i < 8 else 88)
        choir = pitches[:3] if i < 8 else pitches + [pitches[1] + 12, pitches[2] + 12]
        for pitch in sorted(set(choir)):
            add('choir', bar * 4, 3.7, pitch, 64 if i < 8 else 80)
    # A short tom flourish into the harmonic lift and into the ending.
    for bar in [27, 31]:
        for offset, pitch, velocity in [(2.5, 50, 83), (3, 47, 91), (3.5, 45, 100), (3.75, 43, 108)]:
            add('drums', bar * 4 + offset, .15, pitch, velocity)

    # 1:04–1:12: decisive E-major arrival, then the opening gesture returns resolved.
    for key, velocity in [('strings', 82), ('brass', 94), ('choir', 77)]:
        chord(key, 32, 'E', velocity, duration=5.5)
    add('bass', 128, 4, 40, 105)
    add('drums', 128, .15, 36, 113)
    add('drums', 128, 1.5, 49, 98)
    add('lead', 128, 3.7, 76, 113)
    for beat, pitch, velocity in [(134, 76, 66), (136, 83, 56), (138, 87, 58), (140, 88, 70)]:
        add('celesta', beat, 1.6, pitch, velocity)
    for pitch in [52, 59, 68]:
        add('strings', 136, 7.5, pitch, 43)

    sections = [
        (0, 'The signal', 'Dark drone, distant choir, and the main motif in bells.'),
        (16, 'Fragile theme', 'The main motif on a soft solo flute.'),
        (32, 'Lyrical answer', 'A contrasting flowing phrase over sparse harmony.'),
        (48, 'The gathering', 'Accepted dark pulse returns; orchestration and register rise.'),
        (79, 'Hold your breath', 'A half-second break before the full theme.'),
        (80, 'First arrival', 'Heroic lead with the lyrical phrase beneath it.'),
        (96, 'Answer and expansion', 'The motif repeats over a stronger dominant cadence.'),
        (112, 'Harmonic lift', 'E major, broader choir, and the largest instrumental range.'),
        (128, 'Afterglow', 'Decisive E-major chord; the opening bell gesture resolves.'),
        (144, 'Release', 'Natural instrument and hall decay.'),
    ]
    for track in tracks.values():
        track['notes'].sort()
    project = dict(title='Proteus — The signal — proof v01', bpm=120, beats=144,
                   tail_seconds=6, sample_rate=48000, master_gain=.65, reverb_percent=17,
                   sections=[dict(beat=b, seconds=b / 2, name=n, description=d)
                             for b, n, d in sections], tracks=list(tracks.values()))
    validate(project)
    return project


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=HERE / 'proof-v01.json')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('choose a new --output path; existing scores are preserved')
    project = make_project()
    text = json.dumps(project, indent=2, ensure_ascii=False)
    text = re.sub(r'\[\n\s+([\d.]+),\n\s+([\d.]+),\n\s+(\d+),\n\s+(\d+)\n\s+\]',
                  r'[\1, \2, \3, \4]', text)
    args.output.write_text(text + '\n')
    print(f'{args.output}: {len(project["tracks"])} tracks, '
          f'{sum(len(t["notes"]) for t in project["tracks"])} notes, 78 seconds')


if __name__ == '__main__':
    main()
