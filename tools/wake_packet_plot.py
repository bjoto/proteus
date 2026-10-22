#!/usr/bin/env python3
"""Plot packet captures with the same per-time height scale, using the stdlib only.

Usage: wake_packet_plot.py coarse.csv fine.csv output_prefix
Writes a PNG contact sheet and a labelled, standalone SVG companion.
"""
import base64
import csv
import html
import math
from pathlib import Path
import struct
import sys
import zlib


def read(path):
    with path.open(newline='') as stream:
        return {(float(r['time']), int(r['x']), int(r['z'])):
                (float(r['reference']), float(r['packet'])) for r in csv.DictReader(stream)}


def plot(coarse_path, fine_path, output):
    coarse, fine = read(coarse_path), read(fine_path)
    if coarse.keys() != fine.keys():
        raise ValueError('capture coordinates differ')
    times = sorted({t for t, _, _ in fine})
    xs = sorted({x for _, x, _ in fine})
    zs = sorted({z for _, _, z in fine})
    scale, margin = 3, 40
    panel_width, panel_height = len(xs)*scale, len(zs)*scale
    width, height = 3*panel_width, len(times)*(panel_height+margin)
    pixels = bytearray([255])*(width*height*3)
    labels = []
    for row, t in enumerate(times):
        limit = max(abs(fine[t, x, z][0]) for z in zs for x in xs)
        if not math.isfinite(limit) or limit <= 0:
            raise ValueError('invalid height scale')
        titles = ('C solver', coarse_path.stem, fine_path.stem)
        for col, title in enumerate(titles):
            labels.append((col*panel_width+8, row*(panel_height+margin)+18,
                           f'{title}: t={t:g}s, +/-{limit:.4g} m'))
            for zi, z in enumerate(zs):
                for xi, x in enumerate(xs):
                    v = (fine[t, x, z][0], coarse[t, x, z][1], fine[t, x, z][1])[col]
                    if not math.isfinite(v):
                        raise ValueError('nonfinite capture')
                    v = max(-1, min(1, v/limit))
                    color = bytes((round(255*(1+min(v, 0))), round(255*(1-abs(v))),
                                   round(255*(1-max(v, 0)))))
                    for dy in range(scale):
                        offset = (((row*(panel_height+margin)+margin+zi*scale+dy)*width
                                   + col*panel_width+xi*scale)*3)
                        pixels[offset:offset+3*scale] = color*scale
    raw = b''.join(b'\0'+pixels[y*width*3:(y+1)*width*3] for y in range(height))

    def chunk(kind, data):
        return (struct.pack('!I', len(data))+kind+data
                + struct.pack('!I', zlib.crc32(kind+data)))

    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('!2I5B', width, height, 8, 2, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(raw))+chunk(b'IEND', b''))
    output.with_suffix('.png').write_bytes(png)
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height+30}">',
           '<rect width="100%" height="100%" fill="white"/>',
           f'<image width="{width}" height="{height}" href="data:image/png;base64,'
           + base64.b64encode(png).decode()+'"/>']
    for x, y, label in labels:
        svg.append(f'<text x="{x}" y="{y}" font-family="sans-serif" font-size="14">'
                   + html.escape(label)+'</text>')
    svg.append(f'<text x="8" y="{height+20}" font-family="sans-serif" font-size="14">'
               f'X={xs[0]}..{xs[-1]} m, Z={zs[0]}..{zs[-1]} m (downward); '
               'blue=trough, white=zero, red=crest; colors saturate at the reference peak.</text>')
    output.with_suffix('.svg').write_text('\n'.join(svg+['</svg>']))


if __name__ == '__main__':
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    plot(*(Path(p) for p in sys.argv[1:]))
