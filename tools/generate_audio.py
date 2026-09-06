#!/usr/bin/env python3
"""Generate an original sample pack and a small, editable FastTracker II XM."""

from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path

RATE = 16_000
ROOT = Path(__file__).resolve().parents[1]
SAMPLE_DIR = ROOT / "assets" / "samples"
MUSIC_DIR = ROOT / "assets" / "music"


def clamp(value: float) -> int:
    return max(-127, min(127, round(value * 127)))


def make_samples() -> dict[str, list[int]]:
    rng = random.Random(0x50524F54)

    def synth(seconds: float, fn) -> list[int]:
        return [clamp(fn(i / RATE, i)) for i in range(int(seconds * RATE))]

    kick = synth(0.38, lambda t, _:
        math.sin(2*math.pi*(92*t - 62*t*t))*math.exp(-11*t))
    snare = synth(0.28, lambda t, i:
        (0.78*rng.uniform(-1, 1) + 0.22*math.sin(2*math.pi*185*t))
        * math.exp(-15*t) * (1 if i % 2 else 0.92))
    hat = synth(0.075, lambda t, _:
        rng.uniform(-1, 1)*math.exp(-48*t))
    bass = synth(0.48, lambda t, _:
        (0.72*math.sin(2*math.pi*130.81*t) +
         0.20*math.sin(2*math.pi*261.62*t))*math.exp(-3.2*t))
    lead = synth(0.34, lambda t, _:
        ((2*((130.81*t) % 1)-1)*0.55 + math.sin(2*math.pi*130.81*t)*0.25)
        * math.exp(-4.0*t))
    return {"kick": kick, "snare": snare, "hat": hat, "bass": bass, "synth": lead}


def write_wav(path: Path, samples: list[int]) -> None:
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(1)
        out.setframerate(RATE)
        out.writeframes(bytes(sample + 128 for sample in samples))


def fixed(text: str, length: int) -> bytes:
    return text.encode("ascii")[:length].ljust(length, b"\0")


def delta_encode(samples: list[int]) -> bytes:
    previous = 0
    encoded = bytearray()
    for sample in samples:
        encoded.append((sample - previous) & 0xFF)
        previous = sample
    return bytes(encoded)


def event(note=0, instrument=0, volume=0, effect=0, parameter=0) -> bytes:
    return bytes((note, instrument, volume, effect, parameter))


def pattern(index: int, channels: int = 8, rows: int = 64) -> bytes:
    cells = [[event() for _ in range(channels)] for _ in range(rows)]
    roots = [37, 37, 40, 35, 37, 42, 40, 35]  # C-3, Eb-3, F-3, Bb-2
    root = roots[index % len(roots)]

    for row in range(rows):
        step = row % 16
        if step in (0, 4, 8, 12):
            cells[row][0] = event(49, 1)  # kick at C-4
        if index >= 1 and step in (4, 12):
            cells[row][1] = event(49, 2)
        hat_interval = 4 if index == 0 else 2
        if row % hat_interval == 0:
            cells[row][2] = event(49, 3, 0x28 if row % 4 else 0x38)
        if index >= 1 and step in (0, 3, 6, 8, 11, 14):
            bass_note = root + (7 if step in (6, 14) else 0)
            cells[row][3] = event(bass_note, 4, 0x38)
        if index >= 2 and index != 4 and step == 0:
            cells[row][4] = event(root + 12, 5, 0x28)
            cells[row][5] = event(root + 15, 5, 0x22)
            cells[row][6] = event(root + 19, 5, 0x22)
        if index in (3, 5, 6, 7) and row % 4 == 2:
            motif = (0, 3, 7, 10, 7, 3, 12, 10)
            note = root + 24 + motif[(row // 4 + index) % len(motif)]
            cells[row][7] = event(note, 5, 0x30)

    # A small fill at the end of every second pattern.
    if index % 2:
        for row in range(56, 64, 2):
            cells[row][1] = event(49, 2, 0x20 + (row - 56)*3)

    data = b"".join(cell for row in cells for cell in row)
    return struct.pack("<IBHH", 9, 0, rows, len(data)) + data


def instrument(name: str, samples: list[int]) -> bytes:
    header = bytearray()
    header += struct.pack("<I", 263)
    header += fixed(name, 22) + b"\0" + struct.pack("<H", 1)
    header += struct.pack("<I", 40)            # sample header size
    header += bytes(96)                         # keymap: sample zero
    header += bytes(48 + 48)                    # volume/pan envelope points
    header += bytes(14)                         # envelope/vibrato controls
    header += struct.pack("<HH", 0, 0)         # fadeout, reserved
    header += bytes(20)                         # FT2 reserved bytes
    assert len(header) == 263

    sample_header = struct.pack(
        "<IIIBbBBbB22s",
        len(samples), 0, 0, 56, 0, 0, 128, 11, 0, fixed(name, 22)
    )
    assert len(sample_header) == 40
    return bytes(header) + sample_header + delta_encode(samples)


def write_xm(path: Path, samples: dict[str, list[int]]) -> None:
    names = ["kick", "snare", "hat", "bass", "synth"]
    orders = bytes(range(8)).ljust(256, b"\0")
    song_header = struct.pack("<8H", 8, 0, 8, 8, len(names), 1, 6, 125) + orders
    assert len(song_header) == 272

    out = bytearray(b"Extended Module: ")
    out += fixed("Proteus starter", 20) + b"\x1a"
    out += fixed("Proteus generator", 20)
    out += struct.pack("<HI", 0x0104, 276)
    out += song_header
    for index in range(8):
        out += pattern(index)
    for name in names:
        out += instrument(name, samples[name])
    path.write_bytes(out)


def main() -> None:
    SAMPLE_DIR.mkdir(parents=True, exist_ok=True)
    MUSIC_DIR.mkdir(parents=True, exist_ok=True)
    samples = make_samples()
    for name, data in samples.items():
        write_wav(SAMPLE_DIR / f"{name}.wav", data)
    write_xm(MUSIC_DIR / "starter.xm", samples)
    print(f"Generated {len(samples)} WAV samples and {MUSIC_DIR / 'starter.xm'}")


if __name__ == "__main__":
    main()
