#!/usr/bin/env python3
"""Transfer the alternate score to editable XM notes and multisampled instruments.

First prepare/render the dry sample projects, then export to a fresh directory.
The original mix and hand-edited tracker revisions are never overwritten.
"""

import argparse
import array
from copy import deepcopy
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import wave

from render import validate

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SPEED = 6
TICKS_PER_BEAT = 24


def fixed(text, size):
    return text.encode('ascii', errors='replace')[:size].ljust(size, b'\0')


def fresh(path):
    if path.exists() and (not path.is_dir() or any(path.iterdir())):
        raise ValueError(f'choose a fresh directory; refusing to overwrite {path}')
    path.mkdir(parents=True, exist_ok=True)


def prepare(project, output):
    fresh(output)
    plan = []
    for index, original in enumerate(project['tracks']):
        track = deepcopy(original)
        percussion = track['channel'] == 9
        pulse = track.get('synth', {}).get('kind') == 'pulse'
        held = .5 if percussion else 1 if pulse else 4
        spacing = 2 if percussion else 4 if pulse else 8
        pitches = sorted({note[2] for note in track['notes']})
        track['pan'] = 64
        if 'synth' in track:
            # A repeatable middle-of-song timbre; live filter motion is not an XM feature.
            patch = track['synth']
            cutoff = math.sqrt(patch['cutoff_start'] * patch['cutoff_end'])
            patch.update(cutoff_start=cutoff, cutoff_end=cutoff, motion_depth=0)
        track['notes'] = [[i * spacing * 2, held * 2, pitch, 127]
                          for i, pitch in enumerate(pitches)]
        audition = dict(title=f'XM sample bank: {track["name"]}', bpm=120,
                        beats=len(pitches) * spacing * 2, tail_seconds=3,
                        sample_rate=44100, master_gain=project['master_gain'],
                        reverb_percent=0, tracks=[track])
        validate(audition)
        name = f'{index + 1:02d}-{track["name"].replace(" ", "-")}'
        (output / f'{name}.json').write_text(json.dumps(audition, indent=2) + '\n')
        plan.append(dict(name=name, track_index=index, pitches=pitches,
                         held_seconds=held, spacing_seconds=spacing,
                         percussion=percussion, pulse=pulse))
        print(f'python3 soundtrack/alternate/render.py {output / (name + ".json")} '
              f'--output {output / name} --name samples')
    (output / 'sample-plan.json').write_text(json.dumps(plan, indent=2) + '\n')
    (output / 'source.json').write_text(json.dumps(project, indent=2) + '\n')


def read_samples(bank, entry, output):
    folder = bank / entry['name']
    manifest = json.loads((folder / 'manifest.json').read_text())
    path = folder / 'samples.wav'
    if hashlib.sha256(path.read_bytes()).hexdigest() != manifest['wav_sha256']:
        raise ValueError(f'sample render hash mismatch: {path}')
    with wave.open(str(path)) as audio:
        assert audio.getnchannels() == 2 and audio.getsampwidth() == 2
        rate = audio.getframerate()
        pcm = array.array('h', audio.readframes(audio.getnframes()))
    if sys.byteorder != 'little':
        pcm.byteswap()
    samples = []
    for i, pitch in enumerate(entry['pitches']):
        start = round(i * entry['spacing_seconds'] * rate)
        duration = 1.8 if entry['percussion'] else entry['held_seconds']
        length = round(duration * rate)
        # Fold to mono with common headroom; track balance is already in the render.
        values = [round(.65 * (pcm[2*j] + pcm[2*j+1]))
                  for j in range(start, start + length)]
        if max(map(abs, values)) >= 32767:
            raise ValueError('sample downmix clips; lower common sample gain')
        loop_start = loop_length = 0
        if not entry['percussion']:
            loop_start = round((duration - (.3 if entry['pulse'] else 1)) * rate)
            fade = round(.03 * rate)
            original = values[:]
            for j in range(fade):
                mix = j / (fade - 1)
                values[length - fade + j] = round(
                    original[length - fade + j] * (1 - mix)
                    + original[loop_start + j] * mix)
            loop_start += fade
            loop_length = length - loop_start
        else:
            for j in range(min(441, length)):
                values[-1-j] = round(values[-1-j] * j / 441)
        filename = f'{entry["name"]}-{pitch}.wav'
        data = struct.pack(f'<{length}h', *values)
        with wave.open(str(output / filename), 'wb') as audio:
            audio.setnchannels(1)
            audio.setsampwidth(2)
            audio.setframerate(rate)
            audio.writeframes(data)
        samples.append(dict(pitch=pitch, rate=rate, pcm=values, loop_start=loop_start,
                            loop_length=loop_length, wav=filename))
    return samples


def instrument(name, samples, pan, release, bpm, drum=False):
    header = bytearray(263)
    struct.pack_into('<I22sBH', header, 0, 263, fixed(name, 22), 0, len(samples))
    struct.pack_into('<I', header, 29, 40)
    for key in range(96):
        header[33 + key] = min(range(len(samples)),
                              key=lambda i: abs(samples[i]['pitch'] - (key + 12)))
    if not drum:
        ticks = max(2, round(release * bpm / 2.5))
        points = [(0, 64), (1, 64), (1 + ticks // 2, 16), (1 + ticks, 0)]
        for i, point in enumerate(points):
            struct.pack_into('<HH', header, 129 + i * 4, *point)
        header[225] = len(points)
        header[227] = 1  # Sustain until note-off; sample itself contains its attack.
        header[233] = 3
    sample_headers = bytearray()
    payload = bytearray()
    for sample in samples:
        tuning = round((12 * 128 * math.log2(sample['rate'] / 8363)
                        + (60 - sample['pitch']) * 128) / 8) * 8
        relative = (tuning + 64) // 128
        fine = tuning - relative * 128
        sample_headers += struct.pack('<IIIBbBBbB22s', len(sample['pcm']) * 2,
            sample['loop_start'] * 2, sample['loop_length'] * 2, 64, fine,
            0x11 if sample['loop_length'] else 0x10, round(pan / 127 * 255),
            relative, 0, fixed(f'{name} MIDI{sample["pitch"]}', 22))
        previous = 0
        deltas = array.array('H')
        for value in sample['pcm']:
            deltas.append((value - previous) & 65535)
            previous = value
        if sys.byteorder != 'little':
            deltas.byteswap()
        payload += deltas.tobytes()
    return header + sample_headers + payload


def export(project, bank, output):
    if json.loads((bank / 'source.json').read_text()) != project:
        raise ValueError('sample bank was prepared for a different score')
    plan = json.loads((bank / 'sample-plan.json').read_text())
    fresh(output)
    sample_folder = output / 'samples'
    sample_folder.mkdir()
    instruments = []
    names = []
    mappings = {}
    source_samples = []
    for entry in plan:
        index = entry['track_index']
        track = project['tracks'][index]
        samples = read_samples(bank, entry, sample_folder)
        release = track.get('synth', {}).get('release', .5)
        groups = [[s] for s in samples] if entry['percussion'] else [samples]
        drums = {36: 'Kick', 38: 'Snare', 41: 'Floor tom', 42: 'Closed hat', 43: 'Low tom'}
        for group in groups:
            name = drums[group[0]['pitch']] if entry['percussion'] else track['name']
            instruments.append(instrument(name, group, track['pan'], release,
                                          project['bpm'], entry['percussion']))
            names.append(name)
            for sample in group:
                mappings[index, sample['pitch']] = len(instruments)
                source_samples.append({k: v for k, v in sample.items() if k != 'pcm'})

    # Each lane is monophonic. Reserve release tails and an entire event row so
    # no delayed note, key-off, or chord voice can overwrite another event.
    lanes = []
    channel_guide = []
    transferred = []
    for index, track in enumerate(project['tracks']):
        local = []
        first = len(lanes)
        drum = track['channel'] == 9
        release = 1.8 if drum else track.get('synth', {}).get('release', .5)
        for beat, duration, pitch, velocity in sorted(track['notes']):
            start = round(beat * TICKS_PER_BEAT)
            off = max(start + 1, round((beat + duration) * TICKS_PER_BEAT))
            row, delay = divmod(start, SPEED)
            offrow, offdelay = divmod(off, SPEED)
            until = (start if drum else off) + math.ceil(release * project['bpm'] / 2.5)
            lane = next((lane for lane in local if lane['until'] < row * SPEED), None)
            if lane is None:
                lane = dict(until=-1, cells={})
                local.append(lane)
            lane['until'] = max(until, (offrow + 1) * SPEED if not drum else 0)
            exponent = 1.4 if 'synth' in track else 2
            volume = max(1, min(64, round(64 * (velocity / 127) ** exponent)))
            cell = [pitch - 11, mappings[index, pitch], 0x10 + volume,
                    0xE if delay else 0, 0xD0 + delay if delay else 0]
            if not drum and row == offrow:
                if delay:
                    raise ValueError('delayed short note needs a finer tracker grid')
                cell[3:] = [20, offdelay]
            lane['cells'][row] = cell
            if not drum and offrow != row:
                lane['cells'][offrow] = ([0, 0, 0, 20, offdelay] if offdelay
                                         else [97, 0, 0, 0, 0])
            transferred.append(dict(track=index, pitch=pitch, source_beat=beat,
                                    source_duration=duration, velocity=velocity,
                                    channel=first + local.index(lane) + 1,
                                    start_tick=start, off_tick=None if drum else off))
        lanes.extend(local)
        channel_guide.append(dict(name=track['name'], first=first+1, last=len(lanes)))
    music_channels = len(lanes)
    channels = (music_channels + 2) // 2 * 2  # Extra control lane; FT2 wants even counts.
    if channels > 32:
        raise ValueError(f'{channels} channels exceeds FastTracker II limit')
    total_seconds = project['beats'] * 60 / project['bpm'] + project['tail_seconds']
    total_rows = round(total_seconds * project['bpm'] / 60 * 4)
    boundaries = sorted({round(s['beat'] * 4) for s in project['sections']} | {total_rows})
    ranges = []
    cues = []
    for begin, end in zip(boundaries, boundaries[1:]):
        section = next(s for s in project['sections'] if round(s['beat'] * 4) == begin)
        cues.append(dict(name=section['name'], seconds=section['seconds'],
                         order=len(ranges), row=0))
        for row in range(begin, end, 64):
            ranges.append((row, min(row+64, end)))
    if len(ranges) + 1 > 128:
        raise ValueError('too many patterns for FastTracker II')
    patterns = []
    for begin, end in ranges:
        cells = bytearray()
        for row in range(begin, end):
            for channel in range(channels):
                cell = lanes[channel]['cells'].get(row, [0]*5) if channel < music_channels else [0]*5
                cells += bytes(cell)
        patterns.append(struct.pack('<IBHH', 9, 0, end-begin, len(cells)) + cells)
    # A silent end pattern loops onto itself instead of restarting the composition.
    guard = bytearray(channels * 5)
    guard[music_channels * 5 + 3:music_channels * 5 + 5] = bytes([11, len(patterns)])
    patterns.append(struct.pack('<IBHH', 9, 0, 1, len(guard)) + guard)
    count = len(patterns)
    header = (b'Extended Module: ' + fixed('Below the surface', 20) + b'\x1a'
              + fixed('Proteus XM export', 20) + struct.pack('<HI', 0x104, 276)
              + struct.pack('<8H', count, count-1, channels, count, len(instruments),
                            1, SPEED, round(project['bpm']))
              + bytes(range(count)).ljust(256, b'\0'))
    xm = header + b''.join(patterns) + b''.join(instruments)
    (output / 'below-the-surface.xm').write_bytes(xm)
    report = dict(source='mix-v01.json', duration_seconds=total_seconds, bpm=project['bpm'],
                  speed=SPEED, rows_per_beat=4, ticks_per_beat=TICKS_PER_BEAT,
                  channels=channels, channel_guide=channel_guide, instruments=names,
                  patterns=count, end_hold_order=count-1, cues=cues,
                  notes=transferred, samples=source_samples,
                  xm_sha256=hashlib.sha256(xm).hexdigest(),
                  source_sha256=hashlib.sha256(json.dumps(project, sort_keys=True).encode()).hexdigest(),
                  sample_render_manifests=[json.loads((bank / e['name'] / 'manifest.json').read_text())
                                           for e in plan])
    (output / 'transfer.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'{output}: {len(transferred)} notes, {channels} channels, '
          f'{count} patterns, {len(instruments)} instruments, {total_seconds}s')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=HERE / 'mix-v01.json')
    sub = parser.add_subparsers(dest='action', required=True)
    prep = sub.add_parser('prepare')
    prep.add_argument('--output', type=Path, required=True)
    pack = sub.add_parser('export')
    pack.add_argument('--bank', type=Path, required=True)
    pack.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    project = json.loads(args.project.read_text())
    validate(project)
    if project['bpm'] != 120:
        parser.error('this transfer targets the approved fixed 120 BPM score')
    if args.action == 'prepare':
        prepare(project, args.output)
    else:
        export(project, args.bank, args.output)


if __name__ == '__main__':
    main()
