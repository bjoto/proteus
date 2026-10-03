#!/usr/bin/env python3
"""Render an editable palette project with macOS instruments; export portable MIDI."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import wave

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BANK = Path('/System/Library/Components/CoreAudio.component/Contents/Resources/gs_instruments.dls')
PPQ = 480


def validate(project):
    def number(value, low, high):
        return isinstance(value, (float, int)) and math.isfinite(value) and low <= value <= high

    def postprocess_number(value, low, high):
        return (type(value) in (float, int) and math.isfinite(value)
                and low <= value <= high)

    for field, low, high in [('bpm', 20, 300), ('beats', 1, 4096), ('tail_seconds', 0.1, 30),
                             ('master_gain', 0.001, 1), ('reverb_percent', 0, 100)]:
        if not number(project[field], low, high):
            raise ValueError(f'invalid {field}')
    if project['sample_rate'] not in (44100, 48000):
        raise ValueError('sample_rate must be 44100 or 48000')
    channels = set()
    if not project['tracks']:
        raise ValueError('project needs tracks')
    for track in project['tracks']:
        keys = ['channel', 'volume', 'pan']
        if 'synth' in track:
            patch = track['synth']
            if patch['kind'] not in ('drone', 'pulse') or track['channel'] == 9:
                raise ValueError('custom synth needs a drone/pulse patch and melodic channel')
            for field, low, high in [
                    ('gain', 0.001, 1), ('detune_cents', 0, 30),
                    ('cutoff_start', 30, 8000), ('cutoff_end', 30, 8000),
                    ('motion_hz', 0, 10), ('motion_depth', 0, 0.9), ('drive', 0.1, 10),
                    ('sub_mix', 0, 1), ('attack', 0.001, 10), ('release', 0.01, 10)]:
                if not number(patch[field], low, high):
                    raise ValueError(f'invalid synth {field} in {track["name"]}')
            if patch['release'] > project['tail_seconds']:
                raise ValueError('tail_seconds must accommodate the synth release')
            if 'postprocess' in track:
                postprocess = track['postprocess']
                if not isinstance(postprocess, dict):
                    raise ValueError(f'postprocess must be an object in {track["name"]}')
                ranges = [('drive_db', 0, 36), ('wet_mix', 0, 1),
                          ('low_hz', 30, 1000), ('high_hz', 500, 8000)]
                for field, low, high in ranges:
                    if (field not in postprocess
                            or not postprocess_number(postprocess[field], low, high)):
                        raise ValueError(f'invalid postprocess {field} in {track["name"]}')
                if postprocess['low_hz'] >= postprocess['high_hz']:
                    raise ValueError(f'postprocess low_hz must be below high_hz in {track["name"]}')
        else:
            if 'postprocess' in track:
                raise ValueError(f'postprocess requires a custom synth in {track["name"]}')
            keys.append('program')
        for key in keys:
            maximum = 15 if key == 'channel' else 127
            if type(track[key]) is not int or not 0 <= track[key] <= maximum:
                raise ValueError(f'invalid {key} in {track["name"]}')
        if track['channel'] in channels:
            raise ValueError('each track needs a unique MIDI channel')
        channels.add(track['channel'])
        ends = {}
        for note in sorted(track['notes']):
            if (len(note) != 4 or not number(note[0], 0, project['beats'])
                    or not number(note[1], 1 / PPQ, project['beats'])
                    or type(note[2]) is not int or not 0 <= note[2] <= 127
                    or type(note[3]) is not int or not 1 <= note[3] <= 127
                    or note[0] + note[1] > project['beats']):
                raise ValueError(f'invalid note in {track["name"]}: {note}')
            if note[0] < ends.get(note[2], 0):
                raise ValueError(f'overlapping same-pitch notes in {track["name"]}')
            ends[note[2]] = note[0] + note[1]


def vlq(value):
    result = [value & 127]
    while value >> 7:
        value >>= 7
        result.insert(0, (value & 127) | 128)
    return bytes(result)


def midi_track(events, end):
    result = bytearray()
    previous = 0
    for tick, _, data in sorted(events, key=lambda event: event[:2]):
        result += vlq(tick - previous) + data
        previous = tick
    result += vlq(max(previous, end) - previous) + b'\xff\x2f\x00'
    return b'MTrk' + struct.pack('>I', len(result)) + result


def write_midi(project, path):
    end = round(project['beats'] * PPQ)
    tempo = round(60_000_000 / project['bpm'])
    conductor = [(0, 0, b'\xff\x51\x03' + tempo.to_bytes(3, 'big')),
                 (0, 1, b'\xff\x58\x04\x04\x02\x18\x08')]
    for section in project.get('sections', []):
        name = section['name'].encode('utf-8')
        conductor.append((round(section['beat'] * PPQ), 2, b'\xff\x06' + vlq(len(name)) + name))
    tracks = [midi_track(conductor, end)]
    for track in project['tracks']:
        channel = track['channel']
        label = track['name']
        if 'synth' in track:
            label += ' [custom synth; see project.json]'
        name = label.encode('utf-8')
        events = [(0, 0, b'\xff\x03' + vlq(len(name)) + name)]
        if 'synth' not in track:
            events.append((0, 1, bytes([0xC0 | channel, track['program']])))
        for cc, value in [(7, track['volume']), (10, track['pan']), (91, 0), (93, 0)]:
            events.append((0, 2, bytes([0xB0 | channel, cc, value])))
        for beat, duration, pitch, velocity in track['notes']:
            events.append((round(beat * PPQ), 4, bytes([0x90 | channel, pitch, velocity])))
            events.append((round((beat + duration) * PPQ), 3, bytes([0x80 | channel, pitch, 0])))
        tracks.append(midi_track(events, end))
    path.write_bytes(b'MThd' + struct.pack('>IHHH', 6, 1, len(tracks), PPQ) + b''.join(tracks))


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project', nargs='?', type=Path, default=HERE / 'palette-v01.json')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/soundtrack/palette-v01')
    parser.add_argument('--bank', type=Path, default=BANK)
    parser.add_argument('--name', default='palette', help='output WAV/MIDI filename stem')
    args = parser.parse_args()
    if not args.name or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_'
                            for c in args.name):
        parser.error('--name must contain only letters, digits, hyphens, and underscores')
    project = json.loads(args.project.read_text())
    validate(project)
    if platform.system() != 'Darwin' or not shutil.which('xcrun'):
        parser.error('rendering requires macOS and Xcode command-line tools')
    if not args.bank.is_file():
        parser.error(f'instrument bank not found: {args.bank}')
    output = args.output.resolve()
    # A revision is immutable: choose another directory to compare edits.
    if output.exists() and any(output.iterdir()):
        parser.error(f'output is not empty; choose a new revision directory: {output}')
    output.mkdir(parents=True, exist_ok=True)
    cache = ROOT / 'build/soundtrack/toolchain'
    cache.mkdir(parents=True, exist_ok=True)
    binary = cache / 'render'
    source = HERE / 'render.swift'
    if not binary.exists() or source.stat().st_mtime > binary.stat().st_mtime:
        subprocess.run(['xcrun', 'swiftc', '-O', '-module-cache-path', str(cache / 'modules'),
                        str(source), '-o', str(binary)], check=True)
    snapshot = output / 'project.json'
    snapshot.write_text(json.dumps(project, indent=2) + '\n')
    write_midi(project, output / f'{args.name}.mid')
    wav = output / f'{args.name}.wav'
    subprocess.run([str(binary), str(snapshot), str(args.bank.resolve()), str(wav)], check=True)
    with wave.open(str(wav), 'rb') as audio:
        frames, rate = audio.getnframes(), audio.getframerate()
        assert (audio.getnchannels(), audio.getsampwidth()) == (2, 2)
        raw = audio.readframes(frames)
    pcm = [value[0] for value in struct.iter_unpack('<h', raw)]
    peak = max(map(abs, pcm)) / 32768
    rms = math.sqrt(sum(value * value for value in pcm) / len(pcm)) / 32768
    tail = pcm[-rate * 2:]
    if max(map(abs, tail)) > 328:
        raise ValueError('last second exceeds -40 dBFS; increase tail_seconds')
    manifest = {
        'macos': platform.mac_ver()[0], 'architecture': platform.machine(),
        'swift': subprocess.check_output(['xcrun', 'swiftc', '--version'], text=True,
                                         stderr=subprocess.STDOUT).strip(),
        'bank': str(args.bank.resolve()), 'bank_sha256': sha256(args.bank),
        'renderer_sha256': sha256(source), 'driver_sha256': sha256(Path(__file__)),
        'project_sha256': sha256(snapshot), 'wav_sha256': sha256(wav),
        'duration_seconds': frames / rate, 'peak_dbfs': 20 * math.log10(peak),
        'rms_dbfs': 20 * math.log10(rms),
        'clipped_samples': sum(abs(value) >= 32767 for value in pcm),
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))
    print(f'Listen: afplay {wav}')


if __name__ == '__main__':
    main()
