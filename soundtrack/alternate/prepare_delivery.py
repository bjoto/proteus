#!/usr/bin/env python3
"""Prepare an immutable, validated audio-and-cues delivery directory."""

import argparse
import hashlib
import json
import math
import re
import shutil
import unicodedata
import wave
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def section_id(name):
    normalized = unicodedata.normalize('NFKD', name)
    ascii_name = normalized.encode('ascii', 'ignore').decode('ascii').lower()
    return re.sub(r'[^a-z0-9]+', '-', ascii_name).strip('-')


def build_cues(project, audio_path, render_manifest):
    if not project.get('sections'):
        raise ValueError('score must contain sections')
    bpm = project['bpm']
    if (isinstance(bpm, bool) or not isinstance(bpm, (int, float))
            or not math.isfinite(bpm) or bpm <= 0):
        raise ValueError('score bpm must be positive')
    beats = project.get('beats')
    if (isinstance(beats, bool) or not isinstance(beats, (int, float))
            or not math.isfinite(beats) or beats <= 0):
        raise ValueError('score beats must be positive')
    expected_rate = project['sample_rate']
    with wave.open(str(audio_path), 'rb') as audio:
        if audio.getcomptype() != 'NONE' or audio.getnchannels() != 2 or audio.getsampwidth() != 2:
            raise ValueError('delivery WAV must be uncompressed stereo PCM16')
        sample_rate = audio.getframerate()
        total_frames = audio.getnframes()
    if sample_rate != expected_rate:
        raise ValueError(f'WAV sample rate {sample_rate} does not match score {expected_rate}')

    tail_seconds = project.get('tail_seconds')
    if (isinstance(tail_seconds, bool) or not isinstance(tail_seconds, (int, float))
            or not math.isfinite(tail_seconds) or tail_seconds < 0):
        raise ValueError('score tail_seconds must be finite and nonnegative')
    expected_duration = beats * 60 / bpm + tail_seconds
    duration = total_frames / sample_rate
    if abs(duration - expected_duration) > 1 / sample_rate:
        raise ValueError(f'WAV duration {duration} does not match score duration {expected_duration}')
    if abs(render_manifest.get('duration_seconds', -1) - duration) > 1 / sample_rate:
        raise ValueError('WAV duration does not match render manifest')

    sections = []
    ids = set()
    previous_beat = -1
    previous_frame = 0
    for section in project['sections']:
        name = section.get('name', '').strip()
        beat = section.get('beat')
        seconds = section.get('seconds')
        identifier = section_id(name)
        if not name or not identifier or identifier in ids:
            raise ValueError(f'section name produces an empty or duplicate ID: {name!r}')
        if (isinstance(beat, bool) or not isinstance(beat, (int, float))
                or not math.isfinite(beat) or beat <= previous_beat):
            raise ValueError(f'section beats must be unique and strictly increasing: {name}')
        expected_seconds = beat * 60 / bpm
        if (isinstance(seconds, bool) or not isinstance(seconds, (int, float))
                or not math.isfinite(seconds) or abs(seconds - expected_seconds) > 1e-9):
            raise ValueError(f'section time does not match beat/BPM: {name}')
        if beat < 0 or expected_seconds >= duration:
            raise ValueError(f'section lies outside rendered audio: {name}')
        start_frame = round(expected_seconds * sample_rate)
        end_frame = total_frames
        if len(sections) < len(project['sections']) - 1:
            next_section = project['sections'][len(sections) + 1]
            end_frame = round(next_section['beat'] * 60 / bpm * sample_rate)
        if start_frame != previous_frame or end_frame <= start_frame or end_frame > total_frames:
            raise ValueError(f'invalid section frame range: {name}')
        sections.append({
            'id': identifier,
            'name': name,
            'description': section.get('description', ''),
            'start_beat': beat,
            'start_seconds': expected_seconds,
            'end_seconds': end_frame / sample_rate,
            'start_frame': start_frame,
            'end_frame': end_frame,
        })
        ids.add(identifier)
        previous_beat = beat
        previous_frame = end_frame
    if sections[0]['start_frame'] != 0 or sections[-1]['end_frame'] != total_frames:
        raise ValueError('cue sections must cover the complete rendered audio')

    return {
        'audio_file': 'soundtrack.wav',
        'audio_sha256': sha256(audio_path),
        'sample_rate': sample_rate,
        'channels': 2,
        'sample_format': 'signed 16-bit PCM little-endian',
        'total_frames': total_frames,
        'duration_seconds': duration,
        'tempo_bpm': bpm,
        'sections': sections,
        'harmony': project.get('harmony', []),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--render', type=Path, default=ROOT / 'build/soundtrack/full-v01')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/soundtrack/delivery-v01')
    args = parser.parse_args()
    source = args.render.resolve()
    output = args.output.resolve()
    if output == source or source in output.parents:
        parser.error('output must be outside the render directory')
    if output.exists() and any(output.iterdir()):
        parser.error(f'output is not empty; choose a new delivery directory: {output}')

    audio = source / 'soundtrack.wav'
    midi = source / 'soundtrack.mid'
    project_path = source / 'project.json'
    manifest_path = source / 'manifest.json'
    for path in (audio, midi, project_path, manifest_path):
        if not path.is_file():
            parser.error(f'required render input is missing: {path}')
    project = json.loads(project_path.read_text())
    render_manifest = json.loads(manifest_path.read_text())
    if sha256(project_path) != render_manifest.get('project_sha256'):
        parser.error('project.json hash does not match render manifest')
    actual_audio_hash = sha256(audio)
    if actual_audio_hash != render_manifest.get('wav_sha256'):
        parser.error('WAV hash does not match render manifest')
    cues = build_cues(project, audio, render_manifest)
    if midi.read_bytes()[:4] != b'MThd':
        parser.error('MIDI input has no standard MThd header')

    output.mkdir(parents=True, exist_ok=True)
    copies = {
        'soundtrack.wav': audio,
        'soundtrack.mid': midi,
        'project.json': project_path,
        'render-manifest.json': manifest_path,
    }
    for name, path in copies.items():
        shutil.copy2(path, output / name)
    (output / 'cue-sheet.json').write_text(json.dumps(cues, indent=2) + '\n')
    hashes = {name: sha256(output / name) for name in (*copies, 'cue-sheet.json')}
    delivery_manifest = {
        'source_render': str(source),
        'files_sha256': hashes,
    }
    (output / 'delivery-manifest.json').write_text(json.dumps(delivery_manifest, indent=2) + '\n')
    print(json.dumps(delivery_manifest, indent=2))


if __name__ == '__main__':
    main()
