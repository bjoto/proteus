#!/usr/bin/env python3
"""Capture the playable water-review circuit and assemble comparison artifacts.

Uses only the Python standard library. GPU capture needs a desktop graphics context.
Use --assemble to rebuild sheets/maps from existing captures. Add --sequence for
10 Hz frame sequences and --movies to encode them with ffmpeg when installed.
"""

import argparse
import csv
import html
import json
from pathlib import Path
import re
import shutil
import subprocess

from gpu_review import FONT, draw_text, png_read, png_write

FONT.update({
    "A": (2, 5, 5, 7, 5, 5, 5), "B": (6, 5, 5, 6, 5, 5, 6),
    "E": (7, 4, 4, 6, 4, 4, 7), "H": (5, 5, 5, 7, 5, 5, 5),
    "K": (5, 5, 6, 4, 6, 5, 5), "L": (4, 4, 4, 4, 4, 4, 7),
    "M": (5, 7, 7, 5, 5, 5, 5), "N": (5, 7, 7, 7, 7, 7, 5),
    "O": (2, 5, 5, 5, 5, 5, 2), "Q": (2, 5, 5, 5, 7, 3, 1),
    "R": (6, 5, 5, 6, 5, 5, 5), "S": (7, 4, 4, 7, 1, 1, 7),
    "T": (7, 2, 2, 2, 2, 2, 2), "W": (5, 5, 5, 5, 7, 7, 5),
    "X": (5, 5, 5, 2, 5, 5, 5), "Y": (5, 5, 5, 2, 2, 2, 2),
    "-": (0, 0, 0, 7, 0, 0, 0),
})
STOPS = ("dock", "mouth", "crossing", "south-bank", "exposed",
         "quay-approach", "quay-alongside", "return")
NOTES = {
    "dock": "REFLECTIONS CONNECT WATER AND SHORE",
    "mouth": "CHECK TRANSITION FROM SHELTER TO RIVER",
    "crossing": "WATER FILLS THE VIEW - SURFACE CHARACTER FIRST",
    "south-bank": "EMPTY HORIZON - SCENE COVERAGE IS SEPARATE WORK",
    "exposed": "EXPOSED VIEW IS INSIDE MAP CROP - NO SEA ROUTE YET",
    "quay-approach": "READABLE EDGE - WATERLINE DETAIL CAN FOLLOW",
    "quay-alongside": "KEEP SHORE CONTACT AND HULL CONTRAST",
    "return": "PRESERVE DOCK LANDMARKS AND NAVIGABLE CLEARANCE",
}


def run(command, log):
    with log.open("w") as stream:
        subprocess.run([str(value) for value in command], stdout=stream,
                       stderr=subprocess.STDOUT, check=True, timeout=600)


def sheet(items, output, columns, note=""):
    width, height, label = 320, 200, 16
    rows = (len(items) + columns - 1) // columns
    stride = columns * width
    sheet_height = rows * (height + label) + (label if note else 0)
    pixels = bytearray(bytes((15, 20, 28, 255)) * stride * sheet_height)
    for i, (title, path) in enumerate(items):
        w, h, data = png_read(path)
        if (w, h) != (width, height):
            raise ValueError(f"Unexpected image size: {path}")
        x, y = i % columns * width, i // columns * (height + label)
        draw_text(pixels, stride, x + 4, y + 4, title.upper(), (235, 235, 235))
        for row in range(height):
            start = ((y + label + row) * stride + x) * 4
            pixels[start:start + width * 4] = data[row * width * 4:(row + 1) * width * 4]
    if note:
        draw_text(pixels, stride, 4, rows * (height + label) + 4, note, (255, 190, 90))
    png_write(output, stride, sheet_height, pixels)


def route_map(output, flat, waves):
    # Use the exact imported surface rather than fetching newer geographic data.
    source = Path("assets/geometry/eriksberg/geometry.h").read_text()
    vertices, triangles = source.split("static const Triangle3D waterfront_triangles[] = {")
    points = [tuple(map(float, row)) for row in re.findall(
        r"\{ ([-\d.]+)f, ([-\d.]+)f, ([-\d.]+)f \}", vertices)]
    water = [tuple(map(int, row)) for row in re.findall(
        r"\{ \{ (\d+), (\d+), (\d+) \}, \{ 45, 79, 104, 255 \} \}", triangles)]
    if not points or not water:
        raise ValueError("Imported water layout changed; update the review map reader")

    def project(x, z):
        return (x + 700) / 2, (800 - z) / 2

    svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="825" height="780" '
           'viewBox="0 0 825 780">', '<rect width="825" height="780" fill="#586258"/>']
    for triangle in water:
        xy = [project(points[i][0], points[i][2]) for i in triangle]
        svg.append('<polygon fill="#2d4f68" points="' + " ".join(
            f"{x:.2f},{y:.2f}" for x, y in xy) + '"/>')
    for east in range(-700, 951, 100):
        x, _ = project(east, 0)
        svg.append(f'<path d="M{x},0V700" stroke="#a0b0b0" opacity=".2"/>')
    for north in range(-600, 801, 100):
        _, y = project(0, north)
        svg.append(f'<path d="M0,{y}H825" stroke="#a0b0b0" opacity=".2"/>')
    for rows, color in ((flat, "#ffbd54"), (waves, "#e6f2ff")):
        xy = [project(-float(row["x"]), float(row["z"])) for row in rows]
        svg.append('<polyline fill="none" stroke="' + color + '" stroke-width="2" points="'
                   + " ".join(f"{x:.2f},{y:.2f}" for x, y in xy) + '"/>')
    events = [flat[0]] + [row for prev, row in zip(flat, flat[1:])
                         if row["destination"] != prev["destination"]] + [flat[-1]]
    for i, row in enumerate(events):
        x, y = project(-float(row["x"]), float(row["z"]))
        svg.append(f'<circle cx="{x}" cy="{y}" r="5" fill="#ffbd54"/>'
                   f'<text x="{x + 8}" y="{y - 6}" fill="white" font-size="13">'
                   f'{i}: {html.escape(STOPS[i])}</text>')
    svg.extend(['<rect y="700" width="825" height="80" fill="#101820"/>',
                '<g fill="white" font-family="sans-serif" font-size="14">',
                '<text x="12" y="722">NORTH UP | grid 100 m | east = -simulation X</text>',
                '<text x="12" y="743">Orange: flat | white: waves | rectangle: imported map crop</text>',
                '<text x="12" y="764">Map data (c) OpenStreetMap contributors, ODbL 1.0</text>',
                '</g></svg>'])
    (output / "route.svg").write_text("\n".join(svg))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("build/living-city/water-review"))
    parser.add_argument("--assemble", action="store_true")
    parser.add_argument("--sequence", action="store_true")
    parser.add_argument("--movies", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    if not args.assemble:
        for preset in ("flat", "waves", "cpu-flat", "cpu-waves"):
            if args.sequence and not preset.startswith("cpu"):
                for path in (args.output / preset).glob("frame-*.png"):
                    path.unlink()
            command = ["./build/boat_water_review", "--output", args.output / preset]
            if "waves" in preset:
                command.append("--waves")
            if preset.startswith("cpu"):
                command.extend(("--renderer", "cpu"))
            elif args.sequence:
                command.append("--sequence")
            run(command, args.output / f"{preset}.log")
    for preset in ("flat", "waves", "cpu-flat", "cpu-waves"):
        expected = "waves" if "waves" in preset else "flat"
        if f"preset={expected}" not in (args.output / f"{preset}.log").read_text():
            raise ValueError(f"Capture preset does not match its directory: {preset}")
    for stop in STOPS:
        items = [(f"{stop} {preset} {view}", args.output / preset / f"{stop}-{view}.png")
                 for view in ("close", "default") for preset in ("flat", "waves", "cpu-flat")]
        sheet(items, args.output / f"compare-{stop}.png", 3, NOTES[stop])
    sheet([(f"{stop} {preset}", args.output / preset / f"{stop}-close.png")
           for stop in STOPS for preset in ("flat", "waves")], args.output / "overview.png", 2)
    rows = {}
    for preset in ("flat", "waves", "cpu-flat", "cpu-waves"):
        with (args.output / preset / "route.csv").open() as stream:
            rows[preset] = list(csv.DictReader(stream))
    # Consecutive samples make reflection/wake continuity inspectable without a video player.
    for preset in ("flat", "waves"):
        if not (args.output / preset / "frame-00000.png").exists():
            continue
        for second in (13, 26, 49, 72):
            items = [(f"{preset} {second + i / 10:.1f}",
                      args.output / preset / f"frame-{second * 10 + i:05d}.png")
                     for i in range(8)]
            sheet(items, args.output / f"motion-{preset}-{second}.png", 4)
    route_map(args.output, rows["flat"], rows["waves"])
    summary = {preset: {"seconds": float(data[-1]["time"]),
                        "minimum_sampled_center_clearance": min(float(r["shore_distance"]) for r in data),
                        "maximum_speed": max(float(r["speed"]) for r in data)}
               for preset, data in rows.items()}
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    if args.movies:
        if not shutil.which("ffmpeg"):
            raise RuntimeError("--movies requires ffmpeg")
        for preset in ("flat", "waves"):
            run(["ffmpeg", "-y", "-framerate", 10, "-i",
                 args.output / preset / "frame-%05d.png", "-c:v", "libx264", "-crf", 16,
                 "-pix_fmt", "yuv420p", args.output / f"{preset}.mp4"],
                args.output / f"{preset}-encode.log")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
