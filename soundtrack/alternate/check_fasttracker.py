#!/usr/bin/env python3
"""Independently inspect a Proteus XM transfer against its source score and WAVs."""

import argparse
import array
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import wave


def check(folder, project):
    data = (folder / 'below-the-surface.xm').read_bytes()
    report = json.loads((folder / 'transfer.json').read_text())
    assert hashlib.sha256(data).hexdigest() == report['xm_sha256']
    assert data[:17] == b'Extended Module: ' and struct.unpack_from('<H', data, 58)[0] == 0x104
    orders, restart, channels, patterns, instruments, flags, speed, bpm = struct.unpack_from('<8H', data, 64)
    assert channels <= 32 and channels % 2 == 0 and instruments <= 128 and patterns <= 128
    assert (speed, bpm, flags) == (6, 120, 1)
    assert orders == patterns and restart == orders - 1
    assert data[80:80+orders] == bytes(range(orders))
    offset = 60 + struct.unpack_from('<I', data, 60)[0]
    attacks = []
    releases = []
    instrument_uses = Counter()
    tick = 0
    order_times = []
    for pattern in range(patterns):
        size, packing, rows, byte_count = struct.unpack_from('<IBHH', data, offset)
        assert size == 9 and packing == 0 and 0 < rows <= 256
        assert byte_count == rows * channels * 5  # Exporter uses unpacked cells.
        offset += size
        order_times.append(tick * 2.5 / bpm)
        for row in range(rows):
            for channel in range(1, channels+1):
                note, ins, volume, effect, value = data[offset:offset+5]
                offset += 5
                assert 0 <= note <= 97 and ins <= instruments
                if 1 <= note <= 96:
                    delay = value & 15 if effect == 14 and value >> 4 == 13 else 0
                    assert delay < speed and 0x10 <= volume <= 0x50
                    attacks.append((channel, tick+delay, note+11))
                    instrument_uses[ins] += 1
                if note == 97:
                    releases.append((channel, tick))
                if effect == 20:
                    assert 0 < value < speed
                    releases.append((channel, tick+value))
                if effect == 11:
                    assert pattern == patterns-1 and value == pattern
            tick += speed
    assert order_times[-1] == report['duration_seconds'] == 240
    for cue in report['cues']:
        assert order_times[cue['order']] == cue['seconds']
    expected = []
    for group, track in zip(report['channel_guide'], project['tracks']):
        actual = [(at, pitch) for ch, at, pitch in attacks if group['first'] <= ch <= group['last']]
        expected_track = [(round(beat*24), pitch) for beat, duration, pitch, velocity in track['notes']]
        assert Counter(actual) == Counter(expected_track), track['name']
        expected.extend(expected_track)
    assert len(attacks) == len(expected) == 844
    assert Counter(releases) == Counter((n['channel'], n['off_tick']) for n in report['notes']
                                       if n['off_tick'] is not None)
    for note in report['notes']:
        assert abs(note['start_tick']/48 - note['source_beat']/2) <= 1/96 + 1e-9
        if note['off_tick'] is not None:
            assert abs(note['off_tick']/48 - (note['source_beat']+note['source_duration'])/2) <= 1/96 + 1e-9

    sample_index = 0
    for ins in range(1, instruments+1):
        size = struct.unpack_from('<I', data, offset)[0]
        count = struct.unpack_from('<H', data, offset+27)[0]
        assert size == 263 and 1 <= count <= 16 and instrument_uses[ins] > 0
        assert max(data[offset+33:offset+129]) < count
        if data[offset+233] & 1:
            points = data[offset+225]
            envelope = [struct.unpack_from('<HH', data, offset+129+i*4) for i in range(points)]
            assert 2 <= points <= 12 and envelope[-1][1] == 0
            assert all(0 <= x <= 255 and 0 <= y <= 64 for x,y in envelope)
        offset += size
        headers = [struct.unpack_from('<IIIBbBBbB22s', data, offset+i*40) for i in range(count)]
        offset += count * 40
        for length, loop, loop_length, volume, fine, kind, pan, relative, reserved, name in headers:
            source = report['samples'][sample_index]
            sample_index += 1
            assert length % 2 == 0 and kind in (0x10, 0x11)
            assert loop + loop_length <= length
            assert (loop, loop_length) == (source['loop_start']*2, source['loop_length']*2)
            rate = 8363 * 2 ** ((source['pitch'] - 60 + relative + fine/128) / 12)
            # FT2's finetune table advances in eight-unit steps (6.25 cents).
            assert abs(1200 * math.log2(rate / source['rate'])) <= 3.125
            decoded = array.array('h')
            value = 0
            for delta, in struct.iter_unpack('<h', data[offset:offset+length]):
                value = ((value + delta + 32768) & 65535) - 32768
                decoded.append(value)
            if sys.byteorder != 'little':
                decoded.byteswap()
            with wave.open(str(folder / 'samples' / source['wav'])) as wav:
                assert wav.getframerate() == source['rate']
                assert decoded.tobytes() == wav.readframes(wav.getnframes())
            offset += length
    assert offset == len(data) and sample_index == len(report['samples'])
    print(f'XM verified: {len(attacks)} notes, {len(releases)} key-offs, '
          f'{channels} channels, {patterns} patterns, {instruments} instruments, '
          f'{sample_index} samples, 11 aligned cues, 240 seconds then silent hold.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--project', type=Path, default=Path(__file__).with_name('mix-v01.json'))
    args = parser.parse_args()
    check(args.folder, json.loads(args.project.read_text()))
