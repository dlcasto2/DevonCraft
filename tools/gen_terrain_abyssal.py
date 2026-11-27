#!/usr/bin/env python3
"""Merges Abyssal Transcendence block art into DevonCraft's terrain atlas.

Only the cells listed in CELL_MAP change. Every other cell keeps the original
DevonCraft art, including the cells terrain.cpp paints or tints at runtime.

Steps:
  --extract FILE.html  export the needed 64x64 layers into tools/abyssal/
  (default)            read textures/terrain.png and tools/abyssal/*.png,
                       write textures/terrain.png (512x512 RGBA) and
                       textures/terrain.h (RGB565, format of ConvertImg --format=ngl)

Usage (from the repository root):
  python3 tools/gen_terrain_abyssal.py --extract /path/to/abyssal-transcendence-eagler-ui.html
  python3 tools/gen_terrain_abyssal.py
  python3 tools/gen_terrain_abyssal.py --check   # verify terrain.h matches terrain.png
"""
import argparse
import base64
import os
import re
import sys
import zlib

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
ABYSSAL_DIR = os.path.join(HERE, 'abyssal')
TEXTURES = os.path.join(ROOT, 'textures')

CELL = 32          # DevonCraft atlas cell size (512 / 16)
SRC = 64           # Abyssal layer size
ALPHA_CUTOFF = 52  # matches ConvertImg's transparency rule for terrain.png

# Atlas cell (x, y) -> Abyssal layer name. Cells not listed keep their art.
CELL_MAP = {
    # grass, dirt, stone, sand, cobble, planks
    (0, 0): 'GRASS_TOP', (3, 0): 'GRASS_SIDE', (2, 0): 'DIRT', (1, 0): 'STONE',
    (4, 0): 'PLANKS', (7, 0): 'COBBLE', (3, 1): 'COBBLE', (2, 1): 'SAND',
    (1, 1): 'BEDROCK', (1, 3): None,
    # logs and foliage (oak, spruce, birch)
    (4, 1): 'LOG_SIDE', (5, 1): 'LOG_TOP', (4, 7): 'LOG_SPRUCE',
    (4, 3): 'LEAVES', (5, 3): 'LEAVES', (4, 8): 'LEAVES_SPRUCE',
    (5, 8): 'LEAVES_SPRUCE', (11, 4): 'LEAVES_BIRCH',
    # metal and ore blocks
    (6, 1): 'IRON', (7, 1): 'GOLD', (5, 2): 'OBSIDIAN',
    # snow, ice, sandstone, glowstone
    (2, 4): 'SNOW', (3, 4): 'ICE', (4, 4): 'SNOW_SIDE', (9, 6): 'GLOWSTONE',
    (0, 11): 'SANDSTONE_TOP', (0, 12): 'SANDSTONE', (0, 13): 'SANDSTONE',
    (0, 4): 'WOOL_WHITE',
    # chest, furnace, crafting table
    (10, 1): 'CHEST_TOP', (11, 1): 'CHEST_SIDE', (9, 2): 'CHEST_FRONT',
    (12, 2): 'FURNACE_FRONT', (13, 2): 'FURNACE_SIDE', (14, 3): 'FURNACE_TOP',
    (11, 2): 'CRAFT_TOP', (11, 3): 'CRAFT_SIDE', (12, 3): 'CRAFT_FRONT',
}
# Cells terrain.cpp tints at runtime (makeColor). Their source art must be grey.
GREY_NAMES = {'GRASS_TOP', 'LEAVES', 'LEAVES_SPRUCE', 'LEAVES_BIRCH'}


def extract(html_path):
    """Exports every 64x64 layer named in CELL_MAP from the Abyssal HTML."""
    s = open(html_path, encoding='utf-8', errors='ignore').read()
    i = s.index('const TEX_B64 = {')
    seg = s[i:i + 3000000]
    alb = base64.b64decode(re.search(r'alb:\s*"([A-Za-z0-9+/=]+)"', seg).group(1))
    raw = zlib.decompress(alb)
    t_start = s.index('const T = {')
    table = {k: int(v) for k, v in re.findall(r'(\w+):\s*(\d+)', s[t_start:s.index('};', t_start)])}
    layer_bytes = SRC * SRC * 4
    names = sorted({n for n in CELL_MAP.values() if n})
    os.makedirs(ABYSSAL_DIR, exist_ok=True)
    for name in names:
        layer = table[name]
        data = raw[layer * layer_bytes:(layer + 1) * layer_bytes]
        Image.frombytes('RGBA', (SRC, SRC), data).save(os.path.join(ABYSSAL_DIR, name + '.png'))
    print('exported %d layers to %s' % (len(names), ABYSSAL_DIR))


def downscale(img):
    """64x64 -> 32x32 with alpha-weighted box filter."""
    out = Image.new('RGBA', (CELL, CELL))
    px = img.load()
    for y in range(CELL):
        for x in range(CELL):
            acc = [0, 0, 0]
            a_sum = 0
            for dy in (0, 1):
                for dx in (0, 1):
                    r, g, b, a = px[2 * x + dx, 2 * y + dy]
                    acc[0] += r * a; acc[1] += g * a; acc[2] += b * a
                    a_sum += a
            if a_sum == 0:
                out.putpixel((x, y), (0, 0, 0, 0))
                continue
            a_avg = a_sum // 4
            out.putpixel((x, y), (acc[0] // a_sum, acc[1] // a_sum, acc[2] // a_sum, a_avg))
    return out


def to_grey(img):
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = px[x, y]
            l = (r * 30 + g * 59 + b * 11) // 100
            px[x, y] = (l, l, l, a)
    return img


def rgb565(r, g, b, a):
    if a < ALPHA_CUTOFF:
        return 0
    c = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    return 0x0821 if c == 0 else c


def write_header(img, path):
    w, h = img.size
    vals = [rgb565(*p) for p in img.getdata()]
    lines = ['//Generated from terrain.png (output format: ngl)',
             'static uint16_t terrain_data[] = {']
    for i in range(0, len(vals), 16):
        lines.append(', '.join('0x%x' % v for v in vals[i:i + 16]) + ',')
    lines.append('};')
    lines.append('static TEXTURE terrain{')
    lines.append('.width = %d,' % w)
    lines.append('.height = %d,' % h)
    lines.append('.has_transparency = true,')
    lines.append('.transparent_color = 0,')
    lines.append('.bitmap = terrain_data };')
    with open(path, 'w') as f:
        f.write('\n'.join(lines) + '\n')


def build():
    atlas = Image.open(os.path.join(TEXTURES, 'terrain.png')).convert('RGBA')
    assert atlas.size == (512, 512), atlas.size
    changed = 0
    for (cx, cy), name in CELL_MAP.items():
        if not name:
            continue
        layer = Image.open(os.path.join(ABYSSAL_DIR, name + '.png')).convert('RGBA')
        cell = downscale(layer)
        if name in GREY_NAMES:
            cell = to_grey(cell)
        atlas.paste(cell, (cx * CELL, cy * CELL))
        changed += 1
    atlas.save(os.path.join(TEXTURES, 'terrain.png'))
    write_header(atlas, os.path.join(TEXTURES, 'terrain.h'))
    print('replaced %d cells; wrote textures/terrain.png and textures/terrain.h' % changed)


def check():
    """Confirms terrain.h decodes to terrain.png under the converter's rule."""
    atlas = Image.open(os.path.join(TEXTURES, 'terrain.png')).convert('RGBA')
    src = open(os.path.join(TEXTURES, 'terrain.h')).read()
    body = src[src.index('{') + 1:src.index('};')]
    vals = [int(t.strip(), 16) for t in body.replace('\n', ' ').split(',') if t.strip()]
    expect = [rgb565(*p) for p in atlas.getdata()]
    bad = sum(1 for a, b in zip(vals, expect) if a != b)
    print('terrain.h values=%d mismatches=%d' % (len(vals), bad))
    return bad == 0 and len(vals) == len(expect)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--extract', metavar='HTML')
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    if args.extract:
        extract(args.extract)
    elif args.check:
        sys.exit(0 if check() else 1)
    else:
        build()
