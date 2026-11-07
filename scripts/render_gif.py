#!/usr/bin/env python3
"""Render a simulated PixelDeck demo GIF (docs/images/modes.gif).

This is a Python re-draw of the patterns in src/main.cpp, not a capture of the
device. Keep both in sync when a mode's look changes.

Usage: python3 scripts/render_gif.py   (needs Pillow)
"""
import colorsys
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

N, CELL, PAD = 5, 44, 22
SIZE = N * CELL + 2 * PAD
OUT = Path(__file__).resolve().parent.parent / "docs" / "images" / "modes.gif"

GLYPH = {  # same 5x5 bitmasks as kDigitGlyph, MSB = top-left
    1: 0b0010001100001000010001110, 2: 0b0111000010011100100001110,
    3: 0b0111000010001100001001110, 4: 0b0111001010011100001000010,
    7: 0b0111000010000100010000100,
}
WIFI_COLORS = [0xFF0000, 0xFF6600, 0xFFCC00, 0x00FF00, 0x00CCFF, 0x880088]

rng = random.Random(7)
frames, durations = [], []


def rgb(c):
    return (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF


def hue(h):
    r, g, b = colorsys.hsv_to_rgb(h % 1.0, 1, 1)
    return int(r * 255) << 16 | int(g * 255) << 8 | int(b * 255)


def frame(leds, ms):
    img = Image.new("RGB", (SIZE, SIZE), (18, 18, 20))
    glow = Image.new("RGB", (SIZE, SIZE), (0, 0, 0))
    d, gd = ImageDraw.Draw(img), ImageDraw.Draw(glow)
    for i, c in enumerate(leds):
        x, y = PAD + (i % N) * CELL + CELL // 2, PAD + (i // N) * CELL + CELL // 2
        d.rounded_rectangle((x - 17, y - 17, x + 17, y + 17), 6, fill=(30, 30, 33))
        if c:
            gd.ellipse((x - 16, y - 16, x + 16, y + 16), fill=rgb(c))
            d.ellipse((x - 9, y - 9, x + 9, y + 9), fill=rgb(c))
    img = Image.blend(img, glow.filter(ImageFilter.GaussianBlur(8)), 0.35)
    ImageDraw.Draw(img).text((8, SIZE - 14), "simulated", fill=(90, 90, 95))
    frames.append(img)
    durations.append(ms)


def digit(n, ms=600):
    frame([0xFFFFFF if GLYPH[n] >> (24 - i) & 1 else 0 for i in range(25)], ms)


def random_pattern():
    c1, c2, multi = hue(rng.random()), hue(rng.random()), rng.random() < 0.5
    leds, kind = [0] * 25, rng.randrange(4)
    for i in range(25):
        x, y = i % N, i // N
        if kind == 1:
            leds[i] = c2 if multi and (x + y) % 2 else c1
        elif kind == 2 and (x in (0, 4) or y in (0, 4)):
            leds[i] = c1
        elif kind == 3:
            leds[i] = c1 if (x + y) % 3 == 0 else (c2 if multi else 0)
    if kind == 2 and multi:
        leds[12] = c2
    if kind == 0:
        for k in range(5 + rng.randrange(12)):
            leds[rng.randrange(25)] = c2 if multi and k % 2 else c1
    return leds


frame([0xFF0000] * 25, 400)  # boot probe
digit(1)
for f in range(22):  # mode 1: rainbow
    frame([hue(f / 22 + i / 25) for i in range(25)], 90)
digit(2)
frame(random_pattern(), 1600)  # mode 2: static
digit(3)
for _ in range(8):  # mode 3: animated
    frame(random_pattern(), 200)
digit(4)
frame([0] * 25, 700)  # mode 4: first scan running
aps = sorted(((rng.randint(-95, -35), rng.choice(WIFI_COLORS)) for _ in range(17)), reverse=True)
level = lambda rssi: 100 + (rssi + 100) * 155 // 70  # rssiToLevel
wifi = [0] * 25
for i, (rssi, c) in enumerate(aps):
    r, g, b = rgb(c)
    wifi[i] = (r * level(rssi) // 255) << 16 | (g * level(rssi) // 255) << 8 | b * level(rssi) // 255
frame(wifi, 1800)
digit(1)  # long-press: AP count 17
frame([0] * 25, 150)
digit(7)
frame(wifi, 1200)

OUT.parent.mkdir(parents=True, exist_ok=True)
frames[0].save(OUT, save_all=True, append_images=frames[1:], duration=durations,
               loop=0, optimize=True)
print(f"wrote {OUT} ({OUT.stat().st_size // 1024} KB, {len(frames)} frames)")
