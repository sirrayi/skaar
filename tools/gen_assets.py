#!/usr/bin/env python3
"""Generate Skaar's assets from sources in the repo.

    python3 tools/gen_assets.py          (needs: pip install pillow; macOS also uses iconutil)

Outputs (see docs/assets.md for licenses):
  logo      misc/skaar/skaar-logo-source.png ->
            misc/skaar/skaar-mark.png (clean master), skaar-mark-gold.png (README), skaar.icns, skaar.ico, skaar.png,
            misc/skaar/mark-braille*.txt (terminal intro), data/base/gfx/skaar/mark.tga
  fonts     tools/fonts/* (OFL) -> data/base/fonts/*.tga + *.fontinfo (UI text atlases),
            data/base/gfx/2d/bigchars.tga (console charset)
  trainer   aim-range textures, target sphere (models/skaar/target.md3) + shade, kill sound
  misc      console background, white, dev-grid textures, hit sound
"""
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.join(REPO, "data", "base")
BRAND = os.path.join(REPO, "misc", "skaar")
FONTS = os.path.join(REPO, "tools", "fonts")

MONTSERRAT = os.path.join(FONTS, "montserrat", "Montserrat[wght].ttf")
JBMONO = os.path.join(FONTS, "jetbrainsmono", "JetBrainsMono[wght].ttf")
MANROPE = os.path.join(FONTS, "manrope", "Manrope[wght].ttf")

# Brand stage palette (app icon, console) + the light in-game palette (game/shared/theme.h)
STAGE = (11, 11, 12)
CREAM = (235, 230, 219)
EMBER = (242, 98, 46)
GROUND = (230, 227, 221)  # greyish taupe off-white
INK = (35, 35, 35)
GOLD = (212, 168, 75)  # brand mark colour (README)


def rel(path):
    return os.path.relpath(path, REPO)


def save_tga(img, relpath):
    path = os.path.join(BASE, relpath)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    # uncompressed 32-bit TGA, which every ioq3 renderer path loads
    img.save(path, compression=None)
    print("wrote", rel(path))


def font(path, size, weight):
    f = ImageFont.truetype(path, size)
    f.set_variation_by_axes([weight])
    return f


# ---- logo -------------------------------------------------------------------

def logo_mask():
    """Clean, smooth alpha mask of the mark at high resolution (white = mark).

    The source is a ~400 px raster with slightly rough edges; upscale its alpha,
    blur and re-threshold it, which gives a smooth flat shape that scales well."""
    src = Image.open(os.path.join(BRAND, "skaar-logo-source.png")).convert("RGBA")
    alpha = src.getchannel("A")
    alpha = alpha.crop(alpha.getbbox())
    big = alpha.resize((alpha.width * 6, alpha.height * 6), Image.LANCZOS)
    big = big.filter(ImageFilter.GaussianBlur(4))
    return big.point(lambda v: 255 if v >= 128 else 0)


def fit_mask(mask, box_w, box_h):
    scale = min(box_w / mask.width, box_h / mask.height)
    return mask.resize((max(1, round(mask.width * scale)), max(1, round(mask.height * scale))), Image.LANCZOS)


def colored(mask, rgb):
    img = Image.new("RGBA", mask.size, rgb + (0,))
    img.putalpha(mask)
    return img


def app_icon(mask, size):
    """macOS-style icon: off-black rounded square on the 824/1024 grid, cream mark, stage hairline."""
    s = size / 1024.0
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    inset, radius = 100 * s, 185 * s
    box = (inset, inset, size - inset, size - inset)
    draw.rounded_rectangle(box, radius=radius, fill=STAGE + (255,))
    if size >= 64:
        draw.rounded_rectangle(box, radius=radius, outline=CREAM + (24,), width=max(1, round(3 * s)))
    mark = fit_mask(mask, round(560 * s), round(470 * s))
    img.alpha_composite(colored(mark, CREAM), ((size - mark.width) // 2, (size - mark.height) // 2))
    return img


def braille(mask, rows):
    """Render the mask as Unicode braille (2x4 dots per character)."""
    dots_h = rows * 4
    dots_w = round(mask.width / mask.height * dots_h * 1.0)
    dots_w += dots_w % 2
    small = mask.resize((dots_w, dots_h), Image.BOX)
    bits = [(0, 0, 0x01), (0, 1, 0x02), (0, 2, 0x04), (1, 0, 0x08), (1, 1, 0x10), (1, 2, 0x20), (0, 3, 0x40), (1, 3, 0x80)]
    lines = []
    for cy in range(rows):
        line = ""
        for cx in range(dots_w // 2):
            code = 0
            for dx, dy, bit in bits:
                if small.getpixel((cx * 2 + dx, cy * 4 + dy)) >= 110:
                    code |= bit
            line += chr(0x2800 + code)
        lines.append(line.rstrip("⠀"))
    width = max(len(l) for l in lines)
    return "\n".join(l.ljust(width, "⠀") for l in lines) + "\n"


def logo():
    mask = logo_mask()

    master = fit_mask(mask, 1024, 1024)
    colored(master, (255, 255, 255)).save(os.path.join(BRAND, "skaar-mark.png"))
    print("wrote", rel(os.path.join(BRAND, "skaar-mark.png")))

    gold = fit_mask(mask, 480, 480)
    colored(gold, GOLD).save(os.path.join(BRAND, "skaar-mark-gold.png"))
    print("wrote", rel(os.path.join(BRAND, "skaar-mark-gold.png")))

    app_icon(mask, 512).save(os.path.join(BRAND, "skaar.png"))
    print("wrote", rel(os.path.join(BRAND, "skaar.png")))

    sizes = [16, 24, 32, 48, 64, 128, 256]
    app_icon(mask, 256).save(os.path.join(BRAND, "skaar.ico"), sizes=[(n, n) for n in sizes])
    print("wrote", rel(os.path.join(BRAND, "skaar.ico")))

    if shutil.which("iconutil"):
        with tempfile.TemporaryDirectory() as tmp:
            iconset = os.path.join(tmp, "skaar.iconset")
            os.makedirs(iconset)
            for n in (16, 32, 128, 256, 512):
                app_icon(mask, n).save(os.path.join(iconset, "icon_%dx%d.png" % (n, n)))
                app_icon(mask, n * 2).save(os.path.join(iconset, "icon_%dx%d@2x.png" % (n, n)))
            subprocess.run(["iconutil", "-c", "icns", iconset, "-o", os.path.join(BRAND, "skaar.icns")], check=True)
        print("wrote", rel(os.path.join(BRAND, "skaar.icns")))
    else:
        print("skipped skaar.icns (iconutil is macOS-only)")

    # In-game mark: white so the UI can tint it, centered in a square texture
    tex = Image.new("RGBA", (512, 512), (255, 255, 255, 0))
    m = fit_mask(mask, 480, 480)
    tex.alpha_composite(colored(m, (255, 255, 255)), ((512 - m.width) // 2, (512 - m.height) // 2))
    save_tga(tex, "gfx/skaar/mark.tga")

    for name, rows in (("mark-braille.txt", 9), ("mark-braille-small.txt", 2)):
        with open(os.path.join(BRAND, name), "w", encoding="utf-8") as f:
            f.write(braille(mask, rows))
        print("wrote", rel(os.path.join(BRAND, name)))


# ---- fonts ------------------------------------------------------------------

# ASCII plus the few symbols the UI uses
UI_CHARS = [chr(c) for c in range(32, 127)] + list("·°×–—…←→↑↓")


def font_atlas(name, path, weight, size, pad=6, tabular=False):
    """Proportional font atlas + metrics for the modules' text renderer.

    .fontinfo is plain text:
      skaarfont 1
      image <shader path>   size <px>   atlas <w> <h>   metrics <ascent> <descent> <lineheight>
      glyph <codepoint> <x> <y> <w> <h> <xoff> <yoff> <advance>
    Offsets are relative to the pen position on the baseline, y down, in atlas pixels.
    tabular: digits share the widest digit's advance and are centred in it, so
    timers and scores don't jitter as they count."""
    f = font(path, size, weight)
    ascent, descent = f.getmetrics()
    digit_adv = max(f.getlength(d) for d in "0123456789")

    glyphs = []
    for ch in UI_CHARS:
        l, t, r, b = f.getbbox(ch, anchor="ls")
        w, h = max(1, r - l) + pad * 2, max(1, b - t) + pad * 2
        img = Image.new("L", (w, h), 0)
        if ch != " ":
            ImageDraw.Draw(img).text((pad - l, pad - t), ch, font=f, fill=255, anchor="ls")
        adv, shift = f.getlength(ch), 0
        if tabular and ch.isdigit():
            shift = round((digit_adv - adv) / 2)
            adv = digit_adv
        glyphs.append((ch, img, l - pad + shift, t - pad, adv))

    # shelf packing into a 1024-wide atlas
    atlas_w, x, y, shelf = 1024, 0, 0, 0
    placed = []
    for ch, img, xoff, yoff, adv in sorted(glyphs, key=lambda g: -g[1].height):
        if x + img.width > atlas_w:
            x, y, shelf = 0, y + shelf, 0
        placed.append((ch, img, x, y, xoff, yoff, adv))
        x += img.width
        shelf = max(shelf, img.height)
    atlas_h = 1 << math.ceil(math.log2(y + shelf))

    atlas = Image.new("RGBA", (atlas_w, atlas_h), (255, 255, 255, 0))
    lines = ["skaarfont 1",
             "image fonts/%s" % name,
             "size %d" % size,
             "atlas %d %d" % (atlas_w, atlas_h),
             "metrics %d %d %d" % (ascent, descent, round((ascent + descent) * 1.15))]
    for ch, img, gx, gy, xoff, yoff, adv in sorted(placed, key=lambda g: ord(g[0])):
        glyph = Image.new("RGBA", img.size, (255, 255, 255, 0))
        glyph.putalpha(img)
        atlas.alpha_composite(glyph, (gx, gy))
        lines.append("glyph %d %d %d %d %d %d %d %.2f" % (ord(ch), gx, gy, img.width, img.height, xoff, yoff, adv))

    save_tga(atlas, "fonts/%s.tga" % name)
    info = os.path.join(BASE, "fonts", name + ".fontinfo")
    with open(info, "w") as out:
        out.write("\n".join(lines) + "\n")
    print("wrote", rel(info))


def bigchars():
    # Console/HUD charset: 16x16 grid of 16x16 cells, cell index = byte value.
    # The engine draws small (console) text at 8x16, i.e. squeezed to half
    # width, so each glyph is rendered into an 8x16 cell and stretched 2x
    # horizontally to fill the 16x16 atlas cell.
    cell, grid = 16, 16
    img = Image.new("RGBA", (cell * grid, cell * grid), (0, 0, 0, 0))
    f = font(JBMONO, 13, 400)
    for code in range(32, 127):
        glyph = Image.new("RGBA", (8, 16), (0, 0, 0, 0))
        draw = ImageDraw.Draw(glyph)
        ch = chr(code)
        l, t, r, b = draw.textbbox((0, 0), ch, font=f)
        draw.text(((8 - (r - l)) // 2 - l, 0), ch, font=f, fill=(255, 255, 255, 255))
        glyph = glyph.resize((cell, cell), Image.LANCZOS)
        img.alpha_composite(glyph, ((code % grid) * cell, (code // grid) * cell))
    draw = ImageDraw.Draw(img)
    # 10 and 11 are the console cursor glyphs in q3 charsets: solid and underline
    draw.rectangle((10 * cell + 1, 1, 10 * cell + 14, 14), fill=(255, 255, 255, 255))
    draw.rectangle((11 * cell + 1, 13, 11 * cell + 14, 14), fill=(255, 255, 255, 255))
    save_tga(img, "gfx/2d/bigchars.tga")


# ---- misc placeholders --------------------------------------------------------

def console_bg():
    # Stage ground with a faint vertical falloff
    w, h = 64, 256
    img = Image.new("RGBA", (w, h))
    for y in range(h):
        t = y / (h - 1)
        c = tuple(int(v + 6 * t) for v in STAGE) + (255,)
        for x in range(w):
            img.putpixel((x, y), c)
    save_tga(img, "console.tga")


def cursor():
    # Menu pointer: ink arrow with a thin ground-colored edge so it reads on
    # both the light ground and ink text. Drawn at 4x and downsampled.
    s = 4
    pts = [(2, 2), (2, 25), (8, 19), (12, 28), (16, 26), (12, 18), (20, 18)]
    big = Image.new("RGBA", (32 * s, 32 * s), (0, 0, 0, 0))
    draw = ImageDraw.Draw(big)
    draw.polygon([(x * s, y * s) for x, y in pts], fill=INK + (255,), outline=GROUND + (255,), width=2 * s)
    save_tga(big.resize((32, 32), Image.LANCZOS), "gfx/skaar/cursor.tga")


def white():
    # "white" is referenced by the client for fills; normally provided by a shader script
    save_tga(Image.new("RGBA", (8, 8), (255, 255, 255, 255)), "white.tga")


def grid(name, base, line, major):
    # Dev grid: one repeat per 128 world units (mapc's texture scale), minor
    # lines every 16 units, a brighter border so 128-unit cells read at range
    size = 256
    img = Image.new("RGBA", (size, size), base + (255,))
    draw = ImageDraw.Draw(img)
    for i in range(0, size, 32):
        draw.line((i, 0, i, size - 1), fill=line + (255,))
        draw.line((0, i, size - 1, i), fill=line + (255,))
    draw.rectangle((0, 0, size - 1, size - 1), outline=major + (255,), width=2)
    save_tga(img, "textures/skaar/%s.tga" % name)


def save_wav(samples, relpath, rate=22050):
    path = os.path.join(BASE, relpath)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, v)) * 32000)) for v in samples))
    print("wrote", rel(path))


def hit_tick(rate=22050):
    # Short bright tick: 1.8 kHz sine with a fast exponential decay (~60 ms).
    # Also the engine's mandatory default sound (OpenAL backend errors without it).
    n = int(rate * 0.06)
    return [0.6 * math.sin(2 * math.pi * 1800 * i / rate) * math.exp(-i / (rate * 0.012)) for i in range(n)]


# ---- trainer ----------------------------------------------------------------

def save_md3(relpath, surface_name, shader, verts, normals, tris, sts):
    """Single-frame, single-surface MD3. verts in world units (|v| < 512)."""
    def enc_normal(n):
        x, y, z = n
        lat = math.atan2(y, x)
        lng = math.acos(max(-1.0, min(1.0, z)))
        return (int(round(lat * 256 / (2 * math.pi))) & 255) << 8 | (int(round(lng * 256 / (2 * math.pi))) & 255)

    mins = [min(v[i] for v in verts) for i in range(3)]
    maxs = [max(v[i] for v in verts) for i in range(3)]
    radius = max(math.sqrt(sum(c * c for c in v)) for v in verts)

    surf = bytearray()
    hdr_size = 4 + 64 + 4 * 10
    ofs_tris = hdr_size
    ofs_shaders = ofs_tris + 12 * len(tris)
    ofs_st = ofs_shaders + 68
    ofs_xyz = ofs_st + 8 * len(verts)
    ofs_end = ofs_xyz + 8 * len(verts)
    surf += b"IDP3" + surface_name.encode().ljust(64, b"\0")
    surf += struct.pack("<10i", 0, 1, 1, len(verts), len(tris), ofs_tris, ofs_shaders, ofs_st, ofs_xyz, ofs_end)
    for t in tris:
        surf += struct.pack("<3i", *t)
    surf += shader.encode().ljust(64, b"\0") + struct.pack("<i", 0)
    for st in sts:
        surf += struct.pack("<2f", *st)
    for v, n in zip(verts, normals):
        surf += struct.pack("<3hH", *(int(round(c * 64)) for c in v), enc_normal(n))

    frame = struct.pack("<10f", *mins, *maxs, 0, 0, 0, radius) + b"sphere".ljust(16, b"\0")
    ofs_frames = 4 + 4 + 64 + 4 * 9
    ofs_surfaces = ofs_frames + len(frame)
    data = b"IDP3" + struct.pack("<i", 15) + b"skaar".ljust(64, b"\0")
    data += struct.pack("<9i", 0, 1, 0, 1, 0, ofs_frames, ofs_surfaces, ofs_surfaces, ofs_surfaces + len(surf))
    data += frame + bytes(surf)

    path = os.path.join(BASE, relpath)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    print("wrote", rel(path))


def target_sphere(radius=64.0, rings=16, segments=32):
    """UV sphere, radius 64 (the cgame scales it to the target radius)."""
    verts, normals, sts, tris = [], [], [], []
    for r in range(rings + 1):
        theta = math.pi * r / rings
        for s in range(segments + 1):
            phi = 2 * math.pi * s / segments
            n = (math.sin(theta) * math.cos(phi), math.sin(theta) * math.sin(phi), math.cos(theta))
            verts.append(tuple(radius * c for c in n))
            normals.append(n)
            sts.append((s / segments, r / rings))
    row = segments + 1
    for r in range(rings):
        for s in range(segments):
            a, b = r * row + s, (r + 1) * row + s
            # q3 winds triangles clockwise when seen from outside
            if r > 0:
                tris.append((a, a + 1, b))
            if r < rings - 1:
                tris.append((a + 1, b + 1, b))
    save_md3("models/skaar/target.md3", "sphere", "skaar/target", verts, normals, tris, sts)


def target_shade():
    # Matcap for tcGen environment, kept nearly flat so targets read as matte
    # chalk, not chrome: no highlight, only a faint darkening at the very rim
    size = 128
    img = Image.new("RGBA", (size, size))
    for y in range(size):
        for x in range(size):
            dx, dy = (x + 0.5) / size * 2 - 1, (y + 0.5) / size * 2 - 1
            d = min(1.0, math.sqrt(dx * dx + dy * dy))
            v = int(255 * (1.0 - 0.06 * d ** 4))
            img.putpixel((x, y), (v, v, v, 255))
    save_tga(img, "gfx/skaar/target_shade.tga")


def kill_tick(rate=22050):
    # Two quick rising ticks: 1.6 kHz then 2.4 kHz
    out = []
    for freq, start in ((1600, 0.0), (2400, 0.035)):
        n0 = int(rate * start)
        while len(out) < n0:
            out.append(0.0)
        for i in range(int(rate * 0.07)):
            v = 0.55 * math.sin(2 * math.pi * freq * i / rate) * math.exp(-i / (rate * 0.014))
            if n0 + i < len(out):
                out[n0 + i] += v
            else:
                out.append(v)
    return out


def trainer():
    for name, base, line, major in (
            # light taupe range: the ground stays quiet so the pastel-red targets carry the scene
            ("range_floor", (218, 214, 207), (209, 205, 198), (196, 192, 185)),
            ("range_wall", (232, 229, 223), (224, 221, 215), (212, 208, 202)),
            ("range_ceiling", (238, 236, 231), (232, 230, 225), (222, 219, 213)),
            ("range_pad", (198, 193, 185), (189, 184, 176), (174, 169, 161))):
        grid(name, base, line, major)
    target_sphere()
    target_shade()
    save_wav(kill_tick(), "sound/feedback/kill.wav")


def main():
    for path in (MANROPE, JBMONO):
        if not os.path.exists(path):
            sys.exit("missing font %s (see docs/assets.md)" % rel(path))

    logo()

    font_atlas("manrope-200", MANROPE, 200, 96, tabular=True)
    font_atlas("manrope-300", MANROPE, 300, 48, tabular=True)
    font_atlas("jbmono-400", JBMONO, 400, 32)
    bigchars()

    console_bg()
    cursor()
    white()
    grid("grid_floor", (52, 56, 64), (66, 71, 80), (98, 105, 118))
    grid("grid_wall", (118, 122, 128), (132, 136, 142), (160, 164, 170))
    grid("grid_ceiling", (40, 42, 48), (50, 52, 58), (70, 73, 80))
    grid("grid_block", (196, 120, 52), (208, 134, 66), (232, 166, 98))
    save_wav(hit_tick(), "sound/feedback/hit.wav")
    trainer()


if __name__ == "__main__":
    main()
