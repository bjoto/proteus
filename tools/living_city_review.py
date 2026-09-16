#!/usr/bin/env python3
"""Capture matching GPU city routes and a CPU submission profile for visual review."""

import argparse
from pathlib import Path
import subprocess

from gpu_review import png_read, png_write, draw_text, FONT

FONT.update({
    "3": (7, 1, 1, 7, 1, 1, 7), "4": (5, 5, 5, 7, 1, 1, 1),
    "6": (7, 4, 4, 7, 5, 5, 7), "8": (7, 5, 5, 7, 5, 5, 7),
    "9": (7, 5, 5, 7, 1, 1, 7),
})


def run(args):
    result = subprocess.run([str(a) for a in args], text=True, capture_output=True, timeout=120)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout + result.stderr


def sheet(paths, output, columns):
    images = [png_read(path) for path in paths]
    width, height = images[0][:2]
    cell_height = height + 14
    rows = (len(images) + columns - 1) // columns
    canvas_width = columns * width
    pixels = bytearray(bytes((15, 20, 28, 255)) * canvas_width * rows * cell_height)
    for i, (w, h, data) in enumerate(images):
        if (w, h) != (width, height):
            raise ValueError("Comparison dimensions must match")
        x, y = (i % columns) * width, (i // columns) * cell_height
        label = paths[i].stem.split("-")[1]
        draw_text(pixels, canvas_width, x + 4, y + 3, label, (220, 220, 220))
        for row in range(height):
            start = ((y + 14 + row) * canvas_width + x) * 4
            pixels[start:start + width * 4] = data[row * width * 4:(row + 1) * width * 4]
    png_write(output, canvas_width, rows * cell_height, pixels)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compare", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    logs = []
    for route, times in (("short", (0, 2.5, 5, 10, 15)),
                         ("long", (0, 7.68, 15.36, 23.04, 30.72))):
        for height in (200, 240):
            paths = []
            for time in times:
                path = args.output / f"{route}-{time:g}-{height}.png"
                logs.append(run(["./build/waterfront_storyboard", "--renderer", "gpu",
                                 "--route", route, "--time", time, "--resolution",
                                 f"320x{height}", "--screenshot", path]))
                paths.append(path)
            sheet(paths, args.output / f"{route}-{height}-sheet.png", 1)
            if args.compare:
                pairs = [p for path in paths for p in (args.compare / path.name, path)]
                sheet(pairs, args.output / f"{route}-{height}-comparison.png", 2)
        logs.append(run(["./build/waterfront_storyboard", "--renderer", "gpu",
                         "--route", route, "--time", 10, "--resolution", "320x180",
                         "--screenshot", args.output / f"{route}-10-180.png"]))
    logs.append(run(["./build/proteus", "--renderer", "gpu", "--profile", "cube",
                     "--frames", 923, "--warmup", 120]))
    (args.output / "capture.log").write_text("\n".join(logs))
    print(logs[-1])


if __name__ == "__main__":
    main()
