#!/usr/bin/env python3
"""
OpenTyrian 2000 PumpkinOS port: convert the application icon from PNG to
the 24-bit Windows BMP that pilrc expects for ICONFAMILY resources.

Transparent pixels are replaced by the key colour (0, 255, 0), which the
resource file declares with TRANSPARENT 0 255 0. Only dependency-free
Python is used (no PIL), so only non-interlaced 8-bit RGB/RGBA PNGs are
supported.

usage: png2bmp.py <in.png> <out.bmp> [size]
"""
import struct
import sys
import zlib

KEY = (0, 255, 0)
ALPHA_THRESHOLD = 96


def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file")
    pos = 8
    width = height = 0
    bitdepth = colortype = interlace = 0
    idat = b""
    palette = None
    trns = None
    while pos < len(data):
        length, ctype = struct.unpack(">I4s", data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, bitdepth, colortype, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
        elif ctype == b"PLTE":
            palette = [tuple(chunk[i:i + 3]) for i in range(0, len(chunk), 3)]
        elif ctype == b"tRNS":
            trns = chunk
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break
    if bitdepth != 8 or interlace != 0:
        raise ValueError("only non-interlaced 8-bit PNGs are supported")
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[colortype]
    raw = zlib.decompress(idat)
    stride = width * channels
    rows = []
    prev = bytearray(stride)
    p = 0
    for _ in range(height):
        ftype = raw[p]
        p += 1
        cur = bytearray(raw[p:p + stride])
        p += stride
        for i in range(stride):
            a = cur[i - channels] if i >= channels else 0
            b = prev[i]
            c = prev[i - channels] if i >= channels else 0
            if ftype == 1:
                cur[i] = (cur[i] + a) & 0xFF
            elif ftype == 2:
                cur[i] = (cur[i] + b) & 0xFF
            elif ftype == 3:
                cur[i] = (cur[i] + ((a + b) >> 1)) & 0xFF
            elif ftype == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                if pa <= pb and pa <= pc:
                    pred = a
                elif pb <= pc:
                    pred = b
                else:
                    pred = c
                cur[i] = (cur[i] + pred) & 0xFF
        rows.append(cur)
        prev = cur
    pixels = []
    for row in rows:
        line = []
        for x in range(width):
            px = row[x * channels:(x + 1) * channels]
            if colortype == 6:
                r, g, b, a = px
            elif colortype == 2:
                r, g, b = px
                a = 255
            elif colortype == 4:
                r = g = b = px[0]
                a = px[1]
            elif colortype == 0:
                r = g = b = px[0]
                a = 255
            else:
                r, g, b = palette[px[0]]
                a = trns[px[0]] if trns is not None and px[0] < len(trns) else 255
            line.append((r, g, b, a))
        pixels.append(line)
    return width, height, pixels


def resize(width, height, pixels, size):
    """Box-filter resample to size x size (alpha weighted)."""
    out = []
    for y in range(size):
        y0 = y * height / size
        y1 = (y + 1) * height / size
        line = []
        for x in range(size):
            x0 = x * width / size
            x1 = (x + 1) * width / size
            rs = gs = bs = as_ = ws = 0.0
            sy = int(y0)
            while sy < y1 and sy < height:
                wy = min(y1, sy + 1) - max(y0, sy)
                sx = int(x0)
                while sx < x1 and sx < width:
                    wx = min(x1, sx + 1) - max(x0, sx)
                    w = wx * wy
                    r, g, b, a = pixels[sy][sx]
                    rs += r * a * w
                    gs += g * a * w
                    bs += b * a * w
                    as_ += a * w
                    ws += w
                    sx += 1
                sy += 1
            if as_ > 0:
                line.append((int(rs / as_), int(gs / as_), int(bs / as_), int(as_ / ws)))
            else:
                line.append((0, 0, 0, 0))
        out.append(line)
    return out


def write_bmp(path, size, pixels):
    rowbytes = (size * 3 + 3) & ~3
    image = bytearray()
    for y in range(size - 1, -1, -1):
        row = bytearray()
        for x in range(size):
            r, g, b, a = pixels[y][x]
            if a < ALPHA_THRESHOLD:
                r, g, b = KEY
            elif (r, g, b) == KEY:
                g = 254  # keep opaque pixels off the key colour
            row += bytes((b, g, r))
        row += bytes(rowbytes - len(row))
        image += row
    header = struct.pack("<2sIHHI", b"BM", 14 + 40 + len(image), 0, 0, 14 + 40)
    info = struct.pack("<IiiHHIIiiII", 40, size, size, 1, 24, 0, len(image), 2835, 2835, 0, 0)
    with open(path, "wb") as f:
        f.write(header + info + image)


def main():
    if len(sys.argv) not in (3, 4):
        print(__doc__)
        sys.exit(1)
    size = int(sys.argv[3]) if len(sys.argv) == 4 else 32
    width, height, pixels = read_png(sys.argv[1])
    if (width, height) != (size, size):
        pixels = resize(width, height, pixels, size)
    write_bmp(sys.argv[2], size, pixels)
    print("%s: %dx%d -> %s %dx%d" % (sys.argv[1], width, height, sys.argv[2], size, size))


if __name__ == "__main__":
    main()
