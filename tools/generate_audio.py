#!/usr/bin/env python3
"""Generate an original sample pack and a small, editable FastTracker II XM."""

from __future__ import annotations

import math
import random
import struct
import wave
from dataclasses import dataclass
from pathlib import Path

LEGACY_RATE = 16_000
DRUM_RATE = 44_100
MELODIC_FREQUENCY = 130.81 * (8363 * 2 ** (11 / 12)) / LEGACY_RATE
ROOT = Path(__file__).resolve().parents[1]
SAMPLE_DIR = ROOT / "assets" / "samples"
MUSIC_DIR = ROOT / "assets" / "music"


@dataclass
class Sample:
    pcm: list[int]
    rate: int = 44_100
    bits: int = 16


def bandpass(values: list[float], frequency: float, q: float) -> list[float]:
    """Two-pole, unity-peak bandpass at DRUM_RATE; fresh state for each hit."""
    omega = math.tau * frequency / DRUM_RATE
    alpha = math.sin(omega) / (2 * q)
    b0 = alpha / (1 + alpha)
    a1 = -2 * math.cos(omega) / (1 + alpha)
    a2 = (1 - alpha) / (1 + alpha)
    x1 = x2 = y1 = y2 = 0.0
    out = []
    for x in values:
        y = b0 * (x - x2) - a1 * y1 - a2 * y2
        out.append(y)
        x2, x1, y2, y1 = x1, x, y1, y
    return out


def finish_sample(values: list[float], peak: float, rate: int,
                  attack_seconds: float = 0.0004,
                  release_seconds: float = 0.012) -> Sample:
    """Remove subsonic DC, fade boundaries, and set headroom before 16-bit export."""
    pole = math.exp(-math.tau * 12 / rate)
    previous = state = 0.0
    cleaned = []
    for i, value in enumerate(values):
        state = value - previous + pole * state
        previous = value
        attack = min(1.0, i / (attack_seconds * rate))
        release = min(1.0, (len(values) - 1 - i) / (release_seconds * rate))
        cleaned.append(state * attack * release)
    gain = peak * 32767 / max(abs(value) for value in cleaned)
    return Sample([round(value * gain) for value in cleaned], rate, 16)


def make_drums() -> dict[str, Sample]:
    # Independent seeds keep edits to one instrument from changing the others.
    def noise(seconds: float, seed: int) -> list[float]:
        rng = random.Random(seed)
        return [rng.uniform(-1, 1) for _ in range(round(seconds * DRUM_RATE))]

    beater = bandpass(noise(0.5, 0x4B49434B), 3200, 0.8)
    kick = []
    for i, click in enumerate(beater):
        t = i / DRUM_RATE
        # Integrated exponential sweep: 155 Hz at impact, settling to 49 Hz.
        phase = math.tau * (49 * t + 106 * 0.022 * (1 - math.exp(-t / 0.022)))
        body = math.sin(phase) * math.exp(-t / 0.115)
        body += 0.16 * math.sin(2 * phase) * math.exp(-t / 0.045)
        kick.append(0.88 * math.tanh(1.35 * body)
                    + 0.22 * click * math.exp(-t / 0.007))

    snare_noise = noise(0.34, 0x534E4152)
    wires = bandpass(snare_noise, 5200, 0.65)
    snap = bandpass(snare_noise, 1800, 0.9)
    snare = []
    for i, wire in enumerate(wires):
        t = i / DRUM_RATE
        phase = math.tau * (185 * t + 45 * 0.012 * (1 - math.exp(-t / 0.012)))
        body = 0.48 * math.sin(phase) * math.exp(-t / 0.045)
        body += 0.24 * math.sin(math.tau * 330 * t) * math.exp(-t / 0.028)
        envelope = (1 - math.exp(-t / 0.001)) * math.exp(-t / 0.065)
        snare.append(body + 1.15 * wire * envelope
                     + 0.22 * snap[i] * math.exp(-t / 0.009))

    hat_noise = noise(0.105, 0x484154)
    # Inharmonic sine partials give a metallic texture without aliased square waves.
    partials = (3617, 4523, 5789, 7021, 8863, 11173)
    metal = [sum(math.sin(math.tau * frequency * i / DRUM_RATE + j * 0.7)
                 for j, frequency in enumerate(partials)) / len(partials)
             for i in range(len(hat_noise))]
    shimmer = bandpass([0.65 * n + 0.35 * m for n, m in zip(hat_noise, metal)],
                       8500, 0.75)
    hat = []
    for i, value in enumerate(shimmer):
        t = i / DRUM_RATE
        envelope = 0.75 * math.exp(-t / 0.012) + 0.25 * math.exp(-t / 0.028)
        hat.append(value * envelope)

    return {"kick": finish_sample(kick, 0.90, DRUM_RATE),
            "snare": finish_sample(snare, 0.82, DRUM_RATE),
            "hat": finish_sample(hat, 0.58, DRUM_RATE)}


def make_bass() -> Sample:
    """A plucked, harmonic-rich bass with a stable fundamental and closing brightness."""
    rate = 44_100
    # Match the old bass's audible pitch: its 16 kHz PCM used relative note +11
    # without finetuning. The new sample instead plays at its actual sample rate.
    frequency = MELODIC_FREQUENCY
    values = []
    for i in range(round(0.48 * rate)):
        t = i / rate
        phase = math.tau * frequency * t
        cutoff = 260 + 1540 * math.exp(-t / 0.065)
        tone = 0.0
        # Finite saw/pulse-like harmonic mix; no discontinuous oscillators.
        # All 24 partials remain well below Nyquist, even at the highest bass note.
        for harmonic in range(1, 25):
            weight = (0.65 + (0.35 if harmonic % 2 else 0.0)) / harmonic
            brightness = 1 / math.sqrt(1 + (harmonic * frequency / cutoff) ** 4)
            tone += weight * brightness * math.cos(harmonic * phase)
        body = 0.35 * math.cos(phase) * math.exp(-t / 0.18)
        values.append(0.65 * tone * math.exp(-t / 0.105) + body)
    return finish_sample(values, 0.82, rate, attack_seconds=0.0025,
                         release_seconds=0.045)


def make_synth() -> Sample:
    """A gently detuned, bright pluck shared by the chord stabs and melody."""
    rate = 44_100
    # Keep the centre voice strongest so chords retain clear pitch and definition.
    voices = ((1.0, 0.6), (2 ** (-5 / 1200), 0.2), (2 ** (5 / 1200), 0.2))
    values = []
    for i in range(round(0.44 * rate)):
        t = i / rate
        cutoff = 650 + 1950 * math.exp(-t / 0.085)
        tone = 0.0
        # A finite saw-like spectrum avoids the old ramp oscillator's hard edges.
        # Twenty partials leave headroom below Nyquist at the arrangement's high notes.
        for ratio, gain in voices:
            frequency = MELODIC_FREQUENCY * ratio
            phase = math.tau * frequency * t
            for harmonic in range(1, 21):
                weight = 1 / harmonic ** 1.15
                brightness = 1 / math.sqrt(1 + (harmonic * frequency / cutoff) ** 4)
                tone += gain * weight * brightness * math.cos(harmonic * phase)
        values.append(tone * math.exp(-t / 0.16))
    return finish_sample(values, 0.68, rate, attack_seconds=0.004,
                         release_seconds=0.060)


def make_samples() -> dict[str, Sample]:
    return {**make_drums(), "bass": make_bass(), "synth": make_synth()}


def write_wav(path: Path, sample: Sample) -> None:
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(sample.bits // 8)
        out.setframerate(sample.rate)
        if sample.bits == 16:
            out.writeframes(struct.pack(f"<{len(sample.pcm)}h", *sample.pcm))
        else:
            out.writeframes(bytes(value + 128 for value in sample.pcm))


def fixed(text: str, length: int) -> bytes:
    return text.encode("ascii")[:length].ljust(length, b"\0")


def delta_encode(samples: list[int], bits: int = 8) -> bytes:
    previous = 0
    encoded = bytearray()
    for sample in samples:
        delta = (sample - previous) & ((1 << bits) - 1)
        encoded.extend(delta.to_bytes(bits // 8, "little"))
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


def instrument(name: str, sample: Sample) -> bytes:
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

    # XM expresses C-4 playback rate as a relative note plus 1/128-semitone tuning.
    tuning = round(12 * 128 * math.log2(sample.rate / 8363))
    relative = (tuning + 64) // 128
    fine = tuning - relative * 128
    sample_header = struct.pack(
        "<IIIBbBBbB22s",
        len(sample.pcm) * (sample.bits // 8), 0, 0, 56, fine,
        0x10 if sample.bits == 16 else 0, 128, relative, 0, fixed(name, 22)
    )
    assert len(sample_header) == 40
    return bytes(header) + sample_header + delta_encode(sample.pcm, sample.bits)


def write_xm(path: Path, samples: dict[str, Sample]) -> None:
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
