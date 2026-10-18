#!/usr/bin/env python3
"""Deterministic, original 32-bar boating lullaby; leaves starter assets untouched."""
import math
import struct

from generate_audio import MUSIC_DIR, event, finish_sample, fixed, instrument

BPM = 84


def voice(kind):
    rate = 22050
    values = []
    for i in range(rate * 3):
        t = i / rate
        phase = math.tau * 261.625565 * t  # Sample plays in tune at XM C-4.
        if kind == "keys":
            value = (math.sin(phase) + .22 * math.sin(2 * phase) * math.exp(-t / .22)
                     + .07 * math.sin(3 * phase) * math.exp(-t / .12))
            value *= math.exp(-t / .55)
        elif kind == "hum":
            value = (math.sin(phase) + .10 * math.sin(2 * phase))
            value *= (1 - math.exp(-t / .075)) * math.exp(-t / .65)
        else:
            value = (math.sin(phase) + .16 * math.sin(2 * phase)) * math.exp(-t / .5)
        values.append(value)
    return finish_sample(values, .6, rate, attack_seconds=.012, release_seconds=.15)


def pattern(index):
    cells = [[event() for _ in range(8)] for _ in range(64)]
    # C6 / G6 / Fmaj7 / C6 / Am7 / F6 / Dm7 / G: two bars per chord.
    chords = [(37, (53, 56, 58)), (44, (51, 56, 60)),
              (42, (53, 58, 61)), (37, (53, 56, 58)),
              (46, (53, 56, 61)), (42, (54, 58, 63)),
              (39, (54, 58, 61)), (44, (51, 56, 60))]
    # Eight short, deliberately childlike call/answer phrases in C major.
    melodies = [(65, 68, 70, 68), (63, 65, 68, 0),
                (65, 66, 70, 66), (68, 65, 63, 0),
                (68, 70, 73, 70), (66, 65, 61, 0),
                (66, 65, 63, 61), (63, 68, 63, 0)]
    for half in range(2):
        chord_index = (index * 2 + half) % 8
        bass, chord = chords[chord_index]
        start = half * 32
        cells[start][0] = event(bass, 3, 0x2c)
        cells[start + 16][0] = event(bass + 7, 3, 0x26)
        for beat in range(0, 32, 8):
            for channel, note in enumerate(chord, 1):
                cells[start + beat + channel - 1][channel] = event(note, 1, 0x26, 8, (88, 168, 128)[channel - 1])
        phrase = melodies[chord_index]
        if index >= 4:
            phrase = (phrase[0], phrase[2], phrase[1], phrase[3])
        for j, note in enumerate(phrase):
            if note:
                # Second half gently varies the familiar phrase, with space to breathe.
                cells[start + 4 + j * 6][4] = event(note, 2, 0x2c if j == 0 else 0x28)
    data = b"".join(cell for row in cells for cell in row)
    return struct.pack("<IBHH", 9, 0, 64, len(data)) + data


def main():
    names = ("keys", "hum", "bass")
    header = struct.pack("<8H", 8, 0, 8, 8, 3, 1, 6, BPM)
    out = bytearray(b"Extended Module: " + fixed("A little boat", 20) + b"\x1a")
    out += fixed("Proteus generator", 20) + struct.pack("<HI", 0x0104, 276)
    out += header + bytes(range(8)).ljust(256, b"\0")
    for index in range(8):
        out += pattern(index)
    for name in names:
        out += instrument(name, voice(name))
    MUSIC_DIR.mkdir(parents=True, exist_ok=True)
    path = MUSIC_DIR / "a-little-boat.xm"
    path.write_bytes(out)
    print(f"{path}: {32 * 4 * 60 / BPM:.2f}s loop, {BPM} BPM")


if __name__ == "__main__":
    main()
