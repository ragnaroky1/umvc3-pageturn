"""Make a UMvC3 character-select grid icon (.tex) from any image.

Usage:
  python scripts/make_icon.py <image.png|jpg> <CharacterID> [outdir]
  python scripts/make_icon.py --label <CharacterID> [outdir]      # placeholder icon with the name written on it

Output: <outdir>/f_<CharacterID>00_BM_HQ_NOMIP.tex   (default outdir: ./icons_out)
Install location in the game: nativePCx64\\ui\\chs\\chs_face_a\\chs_cs_f\\

Format: MT Framework TEX, 128x128, BC3/DXT5, no mipmaps - identical to the vanilla f_<Name>00 icons.
The image is center-cropped to a square and resized to 128x128. Vanilla icons are a head/upper-body shot.
"""
import sys, os, struct
from PIL import Image, ImageDraw, ImageFont

# 24-byte header copied from a vanilla icon (f_Hatena / f_Ryu00): TEX, version 0x9d, 128x128, BC3, 1 mip
HEADER = bytes.fromhex("544558009da0002001200004012a010018000000" + "00000000")

def _rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

def _from565(c):
    r = (c >> 11) & 31; g = (c >> 5) & 63; b = c & 31
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)

def encode_bc3(img):
    """Simple range-fit BC3 encoder. Good enough for 128x128 UI icons."""
    img = img.convert("RGBA")
    w, h = img.size
    px = img.load()
    out = bytearray()
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            block = [px[bx + x, by + y] for y in range(4) for x in range(4)]
            # alpha block
            a = [p[3] for p in block]
            amax, amin = max(a), min(a)
            if amax == amin:
                out += struct.pack("<BB", amax, amin) + bytes(6)
            else:
                # 8-alpha mode: a0 > a1
                a0, a1 = amax, amin
                pal = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
                idx = [min(range(8), key=lambda k: abs(pal[k] - v)) for v in a]
                bits = 0
                for i, k in enumerate(idx): bits |= k << (3 * i)
                out += struct.pack("<BB", a0, a1) + bits.to_bytes(6, "little")
            # color block: pick the two most distant colors along luminance
            cols = [p[:3] for p in block]
            lum = [0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2] for c in cols]
            cmax = cols[lum.index(max(lum))]; cmin = cols[lum.index(min(lum))]
            c0, c1 = _rgb565(*cmax), _rgb565(*cmin)
            if c0 < c1: c0, c1 = c1, c0; cmax, cmin = cmin, cmax
            if c0 == c1:
                out += struct.pack("<HHI", c0, c1, 0)
                continue
            p0, p1 = _from565(c0), _from565(c1)
            pal = [p0, p1, tuple((2 * p0[i] + p1[i]) // 3 for i in range(3)), tuple((p0[i] + 2 * p1[i]) // 3 for i in range(3))]
            bits = 0
            for i, c in enumerate(cols):
                k = min(range(4), key=lambda j: sum((pal[j][t] - c[t]) ** 2 for t in range(3)))
                bits |= k << (2 * i)
            out += struct.pack("<HHI", c0, c1, bits)
    return bytes(out)

def square_crop_resize(img, size=128):
    w, h = img.size
    s = min(w, h)
    left, top = (w - s) // 2, (h - s) // 2
    return img.crop((left, top, left + s, top + s)).resize((size, size), Image.LANCZOS)

def label_image(name, size=128):
    img = Image.new("RGBA", (size, size), (40, 40, 60, 255))
    d = ImageDraw.Draw(img)
    d.rectangle((2, 2, size - 3, size - 3), outline=(255, 200, 40, 255), width=3)
    try: font = ImageFont.truetype("arialbd.ttf", 22)
    except Exception: font = ImageFont.load_default()
    # wrap long names
    lines, cur = [], ""
    for ch in name:
        cur += ch
        if len(cur) >= 9: lines.append(cur); cur = ""
    if cur: lines.append(cur)
    y = size // 2 - 13 * len(lines)
    for ln in lines:
        bbox = d.textbbox((0, 0), ln, font=font)
        d.text(((size - (bbox[2] - bbox[0])) // 2, y), ln, fill=(255, 255, 255, 255), font=font)
        y += 26
    return img

def write_tex(img, path):
    data = encode_bc3(square_crop_resize(img))
    with open(path, "wb") as f:
        f.write(HEADER + data)

def main():
    args = sys.argv[1:]
    if not args or args[0] in ("-h", "--help"): print(__doc__); return
    if args[0] == "--label":
        cid = args[1]; outdir = args[2] if len(args) > 2 else "icons_out"; img = label_image(cid)
    else:
        img = Image.open(args[0]); cid = args[1]; outdir = args[2] if len(args) > 2 else "icons_out"
    os.makedirs(outdir, exist_ok=True)
    out = os.path.join(outdir, f"f_{cid}00_BM_HQ_NOMIP.tex")
    write_tex(img, out)
    print("wrote", out, os.path.getsize(out), "bytes")

if __name__ == "__main__":
    main()
