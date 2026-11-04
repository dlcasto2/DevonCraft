#!/usr/bin/env python3
"""Refresh the menu/help screenshot labels from the generated DevonCraft UI art."""
from __future__ import annotations

import re
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SCALE = 2  # the checked-in 640x480 captures are 2x the calculator's 320x240 UI
KEY = 0xF81F
GREY_TEXT = 0xA534

def rgb565(value: int) -> tuple[int, int, int]:
    r = (value >> 11) & 31
    g = (value >> 5) & 63
    b = value & 31
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


def extract_array(source: str, declaration: str) -> list[int]:
    match = re.search(declaration + r"\s*=\s*\{(.*?)\};", source, re.S)
    if not match:
        raise ValueError(f"cannot find generated array: {declaration}")
    return [int(token, 16) for token in re.findall(r"0x[0-9A-Fa-f]+", match.group(1))]


def load_title() -> Image.Image:
    source = (ROOT / "gui_art.cpp").read_text(encoding="utf-8")
    data = extract_array(source, r"static COLOR gui_title_data\[\]")
    dims = re.search(r"TEXTURE gui_title = \{\s*(\d+),\s*(\d+),", source)
    if not dims:
        raise ValueError("cannot find gui_title dimensions")
    width, height = map(int, dims.groups())
    if len(data) != width * height:
        raise ValueError("generated title texture size does not match its dimensions")
    image = Image.new("RGBA", (width, height))
    pixels = image.load()
    for y in range(height):
        for x in range(width):
            value = data[x + y * width]
            pixels[x, y] = (0, 0, 0, 0) if value == KEY else (*rgb565(value), 255)
    return image


def load_font() -> tuple[list[int], list[list[int]]]:
    source = (ROOT / "gui_art.cpp").read_text(encoding="utf-8")
    width_match = re.search(r"const uint8_t gui_font_widths\[95\]\s*=\s*\{(.*?)\};", source, re.S)
    if not width_match:
        raise ValueError("cannot find generated gui_font_widths")
    widths = [int(token) for token in re.findall(r"\d+", width_match.group(1))]
    match = re.search(r"const uint8_t gui_font_rows\[95\]\[9\]\s*=\s*\{(.*?)\n\};", source, re.S)
    if not match:
        raise ValueError("cannot find generated gui_font_rows")
    rows = []
    for line in match.group(1).splitlines():
        if "{" not in line:
            continue
        values = [int(token, 16) for token in re.findall(r"0x[0-9A-Fa-f]+", line)]
        if values:
            rows.append(values)
    if len(widths) != 95 or len(rows) != 95 or any(len(row) != 9 for row in rows):
        raise ValueError("generated pixel-font table has unexpected dimensions")
    return widths, rows


def draw_pixel_text(image: Image.Image, text: str, center_x: int, y: int,
                    color: tuple[int, int, int], widths: list[int], rows: list[list[int]]) -> None:
    text_width = sum(widths[ord(ch) - 32] + 1 for ch in text) - 1
    x = center_x - text_width // 2
    draw = ImageDraw.Draw(image)
    for ch in text:
        if not (32 <= ord(ch) <= 126):
            continue
        glyph = ord(ch) - 32
        width = widths[glyph]
        for gy in range(9):
            bits = rows[glyph][gy]
            for gx in range(width):
                if bits & (1 << (4 - gx)):
                    px, py = x + gx, y + gy
                    draw.rectangle((px * SCALE, py * SCALE,
                                    px * SCALE + SCALE - 1, py * SCALE + SCALE - 1), fill=color)
        x += width + 1


def main() -> None:
    menu_path = ROOT / "docs/screenshots/menu.png"
    menu = Image.open(menu_path).convert("RGB")
    bg = menu.getpixel((8, 8))
    draw = ImageDraw.Draw(menu)
    draw.rectangle((0, 0, menu.width - 1, 89), fill=bg)
    title_source = load_title()
    title = title_source.resize((title_source.width * SCALE, title_source.height * SCALE), Image.Resampling.NEAREST)
    menu_rgba = menu.convert("RGBA")
    menu_rgba.alpha_composite(title, ((menu.width - title.width) // 2, 10 * SCALE))
    menu_rgba.convert("RGB").save(menu_path)

    help_path = ROOT / "docs/screenshots/help.png"
    help_image = Image.open(help_path).convert("RGB")
    bg = help_image.getpixel((8, 372))
    draw = ImageDraw.Draw(help_image)
    draw.rectangle((0, 370, help_image.width - 1, 391), fill=bg)
    widths, rows = load_font()
    draw_pixel_text(help_image, "DevonCraft - based on Crafti by Fabian Vogt",
                    center_x=160, y=186, color=rgb565(GREY_TEXT), widths=widths, rows=rows)
    help_image.save(help_path)


if __name__ == "__main__":
    main()
