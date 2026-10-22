#!/usr/bin/env python3
"""Generate Skaar's placeholder 2D assets into data/base.

Everything written here is original or derived from OFL-licensed fonts; see
docs/assets.md. Requires Pillow (pip install pillow).

    python3 tools/gen_placeholder_assets.py [--font /path/to/FiraCode-Regular.ttf]
"""
import argparse
import math
import os
import struct
import sys
import wave

from PIL import Image, ImageDraw, ImageFont

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.join(REPO, "data", "base")

DEFAULT_FONTS = [
    os.path.expanduser("~/Library/Fonts/FiraCode-Regular.ttf"),
    "/usr/share/fonts/truetype/firacode/FiraCode-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
]


def save_tga(img, rel):
    path = os.path.join(BASE, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    # Uncompressed 32-bit TGA, which every ioq3 renderer path loads
    img.save(path, compression=None)
    print("wrote", os.path.relpath(path, REPO))


def bigchars(font_path):
    # Console/HUD charset: 16x16 grid of 16x16 cells, cell index = byte value
    cell, grid = 16, 16
    img = Image.new("RGBA", (cell * grid, cell * grid), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    font = ImageFont.truetype(font_path, 14)
    for code in range(32, 127):
        x, y = (code % grid) * cell, (code // grid) * cell
        ch = chr(code)
        l, t, r, b = draw.textbbox((0, 0), ch, font=font)
        ox = x + (cell - (r - l)) // 2 - l
        oy = y + (cell - 14) // 2 - 1
        draw.text((ox, oy), ch, font=font, fill=(255, 255, 255, 255))
    # 10 and 11 are the console cursor glyphs in q3 charsets: solid and underline
    draw.rectangle((10 * cell + 3, 2, 10 * cell + 12, 13), fill=(255, 255, 255, 255))
    draw.rectangle((11 * cell + 3, 12, 11 * cell + 12, 13), fill=(255, 255, 255, 255))
    save_tga(img, "gfx/2d/bigchars.tga")


def console_bg():
    # Dark slate vertical gradient
    w, h = 64, 256
    img = Image.new("RGBA", (w, h))
    for y in range(h):
        t = y / (h - 1)
        c = (int(14 + 10 * t), int(16 + 12 * t), int(22 + 16 * t), 255)
        for x in range(w):
            img.putpixel((x, y), c)
    save_tga(img, "console.tga")


def white():
    # "white" is referenced by the client for fills; normally provided by a shader script
    save_tga(Image.new("RGBA", (8, 8), (255, 255, 255, 255)), "white.tga")


def save_wav(samples, rel, rate=22050):
    path = os.path.join(BASE, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, v)) * 32000)) for v in samples))
    print("wrote", os.path.relpath(path, REPO))


def hit_tick(rate=22050):
    # Short bright tick: 1.8 kHz sine with a fast exponential decay (~60 ms).
    # Also the engine's mandatory default sound (OpenAL backend errors without it).
    n = int(rate * 0.06)
    return [0.6 * math.sin(2 * math.pi * 1800 * i / rate) * math.exp(-i / (rate * 0.012)) for i in range(n)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--font", help="OFL/free monospace TTF for the console charset")
    args = ap.parse_args()
    font = args.font or next((f for f in DEFAULT_FONTS if os.path.exists(f)), None)
    if not font:
        sys.exit("no font found; pass --font with an OFL-licensed monospace TTF")
    bigchars(font)
    console_bg()
    white()
    save_wav(hit_tick(), "sound/feedback/hit.wav")


if __name__ == "__main__":
    main()
