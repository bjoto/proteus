#!/usr/bin/env python3
"""Generate the small deterministic GPU waterfront material textures."""

from pathlib import Path
import struct
import zlib

SIZE = 32
OUTPUT = Path(__file__).resolve().parents[1] / "assets" / "textures"


def rectangle(pixels, x, y, width, height, color):
    for row in range(y, y + height):
        for column in range(x, x + width):
            pixels[row * SIZE + column] = color


def facade():
    pixels = [(212, 197, 175)] * (SIZE * SIZE)
    for y in (7, 21):
        rectangle(pixels, 0, y, SIZE, 2, (179, 159, 139))
    for y in (2, 11, 23):
        for x in (3, 13, 24):
            rectangle(pixels, x, y, 5, 4 if y == 2 else 5, (89, 78, 81))
    for x, y, width, height in ((4, 3, 3, 2), (15, 12, 2, 3), (25, 24, 3, 3)):
        rectangle(pixels, x, y, width, height, (227, 181, 104))
    return pixels


def roof():
    pixels = [(199, 189, 175)] * (SIZE * SIZE)
    for y in (5, 15, 25):
        rectangle(pixels, 0, y, SIZE, 2, (155, 145, 135))
    for x in (5, 15, 25):
        rectangle(pixels, x, 0, 2, SIZE, (225, 214, 196))
    for x, y, width, height in ((2, 2, 3, 3), (22, 12, 5, 3), (10, 22, 4, 4)):
        rectangle(pixels, x, y, width, height, (117, 110, 104))
    return pixels


def quay():
    pixels = [(189, 183, 170)] * (SIZE * SIZE)
    for x, y in ((0, 0), (16, 7), (0, 14), (16, 21)):
        rectangle(pixels, x, y, 16, 7, (216, 208, 191))
    for y in (6, 20):
        rectangle(pixels, 0, y, SIZE, 1, (147, 143, 136))
    for y, height in ((0, 7), (20, 12)):
        rectangle(pixels, 15, y, 1, height, (147, 143, 136))
    rectangle(pixels, 3, 23, 6, 2, (123, 125, 122))
    rectangle(pixels, 23, 25, 5, 2, (123, 125, 122))
    return pixels


def write_png(path, pixels):
    rows = b"".join(
        b"\0" + bytes(component for pixel in pixels[y * SIZE : (y + 1) * SIZE] for component in pixel)
        for y in range(SIZE)
    )

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0))
    path.write_bytes(png + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, pixels in (("facade.png", facade()), ("roof.png", roof()), ("quay.png", quay())):
        write_png(OUTPUT / name, pixels)


if __name__ == "__main__":
    main()
