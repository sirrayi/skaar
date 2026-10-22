#!/usr/bin/env python3
"""Turn an image into braille or ASCII text art (used for README.md).

    python3 tools/textart.py logo  IMAGE COLS     alpha mask, Floyd-Steinberg dithered braille
    python3 tools/textart.py scene IMAGE COLS     screenshot as braille: edges + ordered-dither
                                                  tones, saturated (target) pixels drawn dense
    python3 tools/textart.py ascii IMAGE COLS     screenshot as a plain ASCII density ramp

Braille packs 2x4 dots per character; lit pixels become dots. Blank cells are
U+2800, not spaces, so every line has the same width in any font.
Needs pillow (pip install pillow).
"""
import sys

from PIL import Image, ImageChops, ImageFilter, ImageOps

RAMP = " .,:;-=+*cxoO8#%@"
BITS = [(0, 0, 0x01), (0, 1, 0x02), (0, 2, 0x04), (1, 0, 0x08),
        (1, 1, 0x10), (1, 2, 0x20), (0, 3, 0x40), (1, 3, 0x80)]
BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def to_braille(dots):
    """1-bit image (width % 2 == 0, height % 4 == 0) -> list of braille lines."""
    px = dots.load()
    lines = []
    for cy in range(dots.height // 4):
        line = ""
        for cx in range(dots.width // 2):
            code = 0
            for dx, dy, bit in BITS:
                if px[cx * 2 + dx, cy * 4 + dy]:
                    code |= bit
            line += chr(0x2800 + code)
        lines.append(line)
    return lines


def dot_size(img, cols):
    w = cols * 2
    return w, max(4, round(img.height / img.width * w / 4) * 4)


def bayer(gray, lo, hi):
    """Ordered dither with brightness mapped to dot density lo..hi (0..1, may clip)."""
    src, out = gray.load(), Image.new("L", gray.size)
    dst = out.load()
    for y in range(gray.height):
        for x in range(gray.width):
            density = lo + (hi - lo) * src[x, y] / 255
            dst[x, y] = 255 if density * 16 > BAYER4[y % 4][x % 4] + 0.5 else 0
    return out


def logo(path, cols):
    """The mark lit from the top left, so the dither shows as a falloff across it."""
    alpha = Image.open(path).convert("RGBA").getchannel("A")
    alpha = alpha.crop(alpha.getbbox())
    mask = alpha.resize(dot_size(alpha, cols), Image.LANCZOS)
    w, h = mask.size
    light = Image.new("L", mask.size)
    light.putdata([round(255 - 200 * ((x / w + y / h) / 2) ** 2.2) for y in range(h) for x in range(w)])
    return to_braille(ImageChops.multiply(mask, light).convert("1"))


def scene(path, cols):
    img = Image.open(path).convert("RGB")
    size = dot_size(img, cols)
    # edges at 3x, cropped so the image border doesn't read as an edge
    big = img.resize((size[0] * 3 + 6, size[1] * 3 + 6), Image.LANCZOS)
    inner = (3, 3, big.width - 3, big.height - 3)
    edges = big.convert("L").filter(ImageFilter.FIND_EDGES).crop(inner)
    edges = edges.resize(size, Image.BOX).point(lambda v: 255 if v > 14 else 0)
    targets = big.convert("HSV").getchannel("S").crop(inner)
    targets = targets.resize(size, Image.BOX).point(lambda v: 255 if v > 110 else 0)
    lum = img.convert("L").resize(size, Image.LANCZOS)
    tone = ImageChops.multiply(bayer(lum, -0.2, 0.8), ImageChops.invert(targets))
    fill = ImageChops.multiply(bayer(lum, 0.35, 1.6), targets)
    return to_braille(ImageChops.lighter(ImageChops.lighter(edges, tone), fill).convert("1"))


def ascii_art(path, cols):
    gray = ImageOps.autocontrast(Image.open(path).convert("L"), cutoff=1)
    h = round(gray.height / gray.width * cols / 2)
    # gamma > 1 keeps mid-grey walls light so the brightest shapes stand out
    small = gray.resize((cols, h), Image.LANCZOS).point(lambda v: round(255 * (v / 255) ** 2.4))
    n = len(RAMP) - 1
    return ["".join(RAMP[small.getpixel((x, y)) * n // 255] for x in range(cols)).rstrip()
            for y in range(h)]


def main(argv):
    modes = {"logo": logo, "scene": scene, "ascii": ascii_art}
    if len(argv) != 4 or argv[1] not in modes:
        sys.exit(__doc__)
    lines = modes[argv[1]](argv[2], int(argv[3]))
    blank = ("", chr(0x2800) * len(lines[0]))
    while lines and lines[0] in blank:
        lines.pop(0)
    while lines and lines[-1] in blank:
        lines.pop()
    print("\n".join(lines))


if __name__ == "__main__":
    main(sys.argv)
