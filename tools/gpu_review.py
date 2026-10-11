#!/usr/bin/env python3
"""Capture and review the CPU and GPU waterfront renderers without image hashes."""

import argparse
import datetime
import json
from pathlib import Path
import struct
import subprocess
import sys
import zlib


TIMES = (0.0, 24.0, 56.0, 88.0, 148.0, 176.0, 216.0, 240.0)
WIDTH = 320
HEIGHT = 200


def png_read(path):
    """Read the non-interlaced RGBA PNGs produced by raylib ExportImage."""
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    position = 8
    chunks = []
    while position < len(data):
        length = struct.unpack(">I", data[position : position + 4])[0]
        kind = data[position + 4 : position + 8]
        value = data[position + 8 : position + 8 + length]
        position += length + 12
        chunks.append((kind, value))
    header = next(value for kind, value in chunks if kind == b"IHDR")
    width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(
        ">IIBBBBB", header
    )
    if (bit_depth, color_type, compression, filtering, interlace) != (8, 6, 0, 0, 0):
        raise ValueError("expected an 8-bit RGBA, non-interlaced PNG")
    compressed = b"".join(value for kind, value in chunks if kind == b"IDAT")
    raw = zlib.decompress(compressed)
    stride = width * 4
    pixels = bytearray(height * stride)
    previous = bytearray(stride)
    offset = 0
    for y in range(height):
        filter_kind = raw[offset]
        offset += 1
        scanline = bytearray(raw[offset : offset + stride])
        offset += stride
        for x in range(stride):
            left = scanline[x - 4] if x >= 4 else 0
            above = previous[x]
            upper_left = previous[x - 4] if x >= 4 else 0
            if filter_kind == 1:
                scanline[x] = (scanline[x] + left) & 255
            elif filter_kind == 2:
                scanline[x] = (scanline[x] + above) & 255
            elif filter_kind == 3:
                scanline[x] = (scanline[x] + ((left + above) // 2)) & 255
            elif filter_kind == 4:
                p = left + above - upper_left
                pa, pb, pc = abs(p - left), abs(p - above), abs(p - upper_left)
                predictor = left if pa <= pb and pa <= pc else above if pb <= pc else upper_left
                scanline[x] = (scanline[x] + predictor) & 255
            elif filter_kind != 0:
                raise ValueError("unknown PNG row filter")
        start = y * stride
        pixels[start : start + stride] = scanline
        previous = scanline
    return width, height, pixels


def png_write(path, width, height, pixels):
    raw = b"".join(b"\0" + pixels[y * width * 4 : (y + 1) * width * 4] for y in range(height))

    def chunk(kind, value):
        return struct.pack(">I", len(value)) + kind + value + struct.pack(">I", zlib.crc32(kind + value) & 0xFFFFFFFF)

    contents = b"\x89PNG\r\n\x1a\n"
    contents += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    contents += chunk(b"IDAT", zlib.compress(raw, 9))
    contents += chunk(b"IEND", b"")
    Path(path).write_bytes(contents)


def command(arguments, label):
    completed = subprocess.run(arguments, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if completed.returncode:
        raise RuntimeError(f"{label} failed ({completed.returncode}):\n{completed.stdout}")
    return completed.stdout


def capture(executable, renderer, time, path):
    arguments = [executable, "--renderer", renderer]
    arguments += ["--screenshot", str(path), "--scene", "cube", "--time", str(time), "--resolution", "320x200"]
    return command(arguments, f"{renderer} capture at t={time:g}")


def semantic_check(path, renderer, time):
    width, height, pixels = png_read(path)
    failures = []
    if (width, height) != (WIDTH, HEIGHT):
        failures.append(f"dimensions are {width}x{height}, expected {WIDTH}x{HEIGHT}")
        return failures, {}
    # Trace the exposed dusk sky from the upper edge. Restrict its colour envelope to
    # indigo/violet/peach and the pale moon; disconnected city lights are not sky.
    def sky_color(index):
        r, g, b = pixels[index * 4 : index * 4 + 3]
        dusk = 14 <= r <= 165 and 22 <= g <= 110 and 45 <= b <= 118
        dusk = dusk and b >= g + 8 and -8 <= r - g <= 58
        moon = 140 <= r <= 224 and 135 <= g <= 216 and 145 <= b <= 194
        return dusk or moon

    sky = set()
    pending = [x for x in range(width) if sky_color(x)]
    while pending:
        index = pending.pop()
        if index in sky or not sky_color(index):
            continue
        sky.add(index)
        x, y = index % width, index // width
        if x > 0:
            pending.append(index - 1)
        if x + 1 < width:
            pending.append(index + 1)
        if y > 0:
            pending.append(index - width)
        if y + 1 < height:
            pending.append(index + width)
    city_pixels = width * height - len(sky)
    bright_lower_pixels = 0
    for index in range(width * (height // 3), width * height):
        pixel = pixels[index * 4 : index * 4 + 3]
        if index not in sky and max(pixel) >= 145 and pixel[0] >= pixel[2]:
            bright_lower_pixels += 1
    coverage = city_pixels / (width * height)
    # City pixels include buildings, quay, traffic, boat and lights. Broad bounds leave room for
    # driver rasterization differences while rejecting a cleared, flipped, or missing city target.
    if len(sky) < 12:
        failures.append(f"exposed dusk sky has only {len(sky)} pixels")
    if not 0.04 <= coverage <= 0.96:
        failures.append(f"city coverage {coverage:.1%} is outside the 4%-96% review range")
    # Lit traffic/boat/lamp regions intentionally move with this route. This checks their broad,
    # lower-frame occupancy rather than prescribing their exact cross-driver rasterization.
    if bright_lower_pixels < 18:
        failures.append(f"moving-object/light region has only {bright_lower_pixels} bright pixels")
    return failures, {
        "renderer": renderer,
        "time_seconds": time,
        "dimensions": [width, height],
        "exposed_sky_pixels": len(sky),
        "city_coverage": round(coverage, 6),
        "moving_object_region_pixels": bright_lower_pixels,
    }


def visual_difference(cpu, gpu):
    width, height, cpu_pixels = png_read(cpu)
    other_width, other_height, gpu_pixels = png_read(gpu)
    if (width, height) != (other_width, other_height):
        raise ValueError("cannot compose differently sized images")
    result = bytearray(len(cpu_pixels))
    for index in range(0, len(result), 4):
        # A modest gain makes palette/composition differences readable without declaring them errors.
        for channel in range(3):
            result[index + channel] = min(255, abs(cpu_pixels[index + channel] - gpu_pixels[index + channel]) * 3)
        result[index + 3] = 255
    return width, height, result


FONT = {
    " ": (0, 0, 0, 0, 0, 0, 0), "|": (2, 2, 2, 2, 2, 0, 2), ".": (0, 0, 0, 0, 0, 2, 2),
    "0": (7, 5, 5, 5, 5, 5, 7), "1": (2, 6, 2, 2, 2, 2, 7), "2": (7, 1, 1, 7, 4, 4, 7),
    "3": (7, 1, 1, 7, 1, 1, 7), "4": (5, 5, 5, 7, 1, 1, 1),
    "6": (7, 4, 4, 7, 5, 5, 7), "8": (7, 5, 5, 7, 5, 5, 7), "9": (7, 5, 5, 7, 1, 1, 7),
    "5": (7, 4, 4, 7, 1, 1, 7), "7": (7, 1, 2, 2, 2, 2, 2),
    "C": (7, 4, 4, 4, 4, 4, 7), "D": (6, 5, 5, 5, 5, 5, 6), "G": (7, 4, 4, 5, 5, 5, 7),
    "F": (7, 4, 4, 6, 4, 4, 4), "P": (6, 5, 5, 6, 4, 4, 4),
    "I": (7, 2, 2, 2, 2, 2, 7),
    "U": (5, 5, 5, 5, 5, 5, 7), "V": (5, 5, 5, 5, 5, 2, 2),
}


def draw_text(pixels, width, x, y, text, color):
    for character in text:
        for row, bits in enumerate(FONT.get(character, FONT[" "])):
            for column in range(3):
                if bits & (1 << (2 - column)):
                    offset = ((y + row) * width + x + column) * 4
                    pixels[offset : offset + 4] = bytes((*color, 255))
        x += 4


def contact_sheet(output, captures):
    label_height, separator = 12, 2
    sheet_width = WIDTH * 3 + separator * 2
    sheet_height = (HEIGHT + label_height) * len(captures)
    sheet = bytearray([0, 0, 0, 255] * sheet_width * sheet_height)
    for row, (time, cpu, gpu) in enumerate(captures):
        _, _, cpu_pixels = png_read(cpu)
        _, _, diff_pixels = visual_difference(cpu, gpu)
        _, _, gpu_pixels = png_read(gpu)
        top = row * (HEIGHT + label_height)
        draw_text(sheet, sheet_width, 2, top + 2, f"CPU {time:g}", (210, 210, 210))
        draw_text(sheet, sheet_width, WIDTH + separator + 2, top + 2, f"GPU {time:g}", (210, 210, 210))
        draw_text(sheet, sheet_width, (WIDTH + separator) * 2 + 2, top + 2, f"DIFF {time:g}", (255, 180, 100))
        for column, image in enumerate((cpu_pixels, gpu_pixels, diff_pixels)):
            destination_x = column * (WIDTH + separator)
            for y in range(HEIGHT):
                source = y * WIDTH * 4
                target = ((top + label_height + y) * sheet_width + destination_x) * 4
                sheet[target : target + WIDTH * 4] = image[source : source + WIDTH * 4]
    png_write(output, sheet_width, sheet_height, sheet)


def cmake_details(build_directory):
    details = {}
    cache = build_directory / "CMakeCache.txt"
    if not cache.exists():
        return details
    for line in cache.read_text(errors="replace").splitlines():
        if line.startswith(("CMAKE_BUILD_TYPE:", "CMAKE_C_COMPILER:", "CMAKE_GENERATOR:",
        "PROTEUS_GPU_TESTS:")):
            key, value = line.split("=", 1)
            details[key.split(":", 1)[0]] = value
    return details


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--proteus", required=True)
    parser.add_argument("--gpu-fixture", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--build-directory", required=True)
    arguments = parser.parse_args()
    output = Path(arguments.output)
    output.mkdir(parents=True, exist_ok=True)

    manifest = {
        "format_version": 1,
        "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "build": cmake_details(Path(arguments.build_directory)),
        "captures": [],
        "fixture": {},
    }
    try:
        fixture_path = output / "gpu-fixture.png"
        fixture_output = command(
            [arguments.gpu_fixture, "--check", "--screenshot", str(fixture_path)], "GPU fixture"
        )
        manifest["fixture"] = {"output": fixture_output, "screenshot": fixture_path.name}
        captures = []
        all_failures = []
        for time in TIMES:
            suffix = f"t{time:g}".replace(".", "_")
            cpu = output / f"cpu-{suffix}.png"
            gpu = output / f"gpu-{suffix}.png"
            capture(arguments.proteus, "cpu", time, cpu)
            capture(arguments.proteus, "gpu", time, gpu)
            for renderer, path in (("cpu", cpu), ("gpu", gpu)):
                failures, report = semantic_check(path, renderer, time)
                manifest["captures"].append({"file": path.name, **report})
                all_failures.extend(f"{renderer} t={time:g}: {failure}" for failure in failures)
            captures.append((time, cpu, gpu))
        contact_sheet(output / "contact-sheet.png", captures)
        manifest["contact_sheet"] = "contact-sheet.png"
        manifest["semantic_result"] = "pass" if not all_failures else "fail"
        manifest["failures"] = all_failures
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        if all_failures:
            raise RuntimeError("semantic checks failed:\n" + "\n".join(all_failures))
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        manifest["result"] = "fail"
        manifest["error"] = str(error)
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(error, file=sys.stderr)
        return 1
    manifest["result"] = "pass"
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"gpu_review=ok\noutput={output}\ncontact_sheet={output / 'contact-sheet.png'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
