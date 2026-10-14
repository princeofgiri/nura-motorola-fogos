#!/usr/bin/env python3
"""Blit a PNG centered on /dev/fb0 (720x1600x32 XRGB8888-LE). Usage: fbshow.py FILE"""
import zlib, struct, sys, os, mmap

FB_W, FB_H = 720, 1600

def get_pitch():
    import fcntl
    try:
        fb = os.open('/dev/fb0', os.O_RDONLY)
        fix = fcntl.ioctl(fb, 0x4602, bytes(128))
        os.close(fb)
        return struct.unpack('I', fix[48:52])[0]
    except Exception as e:
        print(f'pitch ioctl failed ({e}), using {FB_W * 4}', flush=True)
        return FB_W * 4

PITCH = get_pitch()
print(f'pitch: {PITCH}', flush=True)

def read_png(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n', 'not a png'
    pos, w = 8, None
    ctype = depth = None
    interlace = None
    raw = b''
    pal = None
    while pos < len(d):
        (ln,) = struct.unpack('>I', d[pos:pos + 4])
        typ = d[pos + 4:pos + 8]
        chunk = d[pos + 8:pos + 8 + ln]
        if typ == b'IHDR':
            w, h, depth, ctype, comp, filt, interlace = struct.unpack('>IIBBBBB', chunk)
            assert depth == 8 and interlace == 0, f'need 8-bit non-interlaced, got depth={depth} interlace={interlace} ctype={ctype}'
        elif typ == b'PLTE':
            pal = chunk
        elif typ == b'IDAT':
            raw += chunk
        elif typ == b'IEND':
            break
        pos += 12 + ln
    ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    stride = w * ch
    out = bytearray(w * h * ch)
    data = zlib.decompress(raw)
    prev = bytearray(stride)
    p = 0
    for y in range(h):
        f = data[p]; p += 1
        line = bytearray(data[p:p + stride]); p += stride
        if f == 1:
            for i in range(ch, stride):
                line[i] = (line[i] + line[i - ch]) & 0xFF
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif f == 3:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                line[i] = (line[i] + a + prev[i]) & 0xFF
        elif f == 4:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                b = prev[i]
                c = prev[i - ch] if i >= ch else 0
                q = a + b - c
                pa, pb, pc = abs(q - a), abs(q - b), abs(q - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        out[y * stride:(y + 1) * stride] = line
        prev = line
    px = []
    if ctype == 2:
        for i in range(0, len(out), 3):
            px.append((out[i], out[i + 1], out[i + 2]))
    elif ctype == 6:
        for i in range(0, len(out), 4):
            r, g, b, a = out[i], out[i + 1], out[i + 2], out[i + 3]
            # composite over black
            px.append(((r * a) // 255, (g * a) // 255, (b * a) // 255))
    elif ctype == 3:
        assert pal, 'no palette'
        for i in range(len(out)):
            o = out[i] * 3
            px.append((pal[o], pal[o + 1], pal[o + 2]))
    elif ctype == 0:
        for i in range(len(out)):
            px.append((out[i], out[i], out[i]))
    return w, h, px

w, h, px = read_png(sys.argv[1])
print(f'image {w}x{h}', flush=True)
if '--fit' in sys.argv:
    s = min(FB_W / w, FB_H / h)
    nw, nh = max(1, round(w * s)), max(1, round(h * s))
    print(f'scaled to {nw}x{nh}', flush=True)
    out = []
    for y in range(nh):
        sy = min(h - 1, int((y + 0.5) / s))
        for x in range(nw):
            sx = min(w - 1, int((x + 0.5) / s))
            out.append(px[sy * w + sx])
    w, h, px = nw, nh, out
fb = os.open('/dev/fb0', os.O_RDWR)
size = PITCH * FB_H
mm = mmap.mmap(fb, size, access=mmap.ACCESS_WRITE)
mm[:] = b'\x00' * size
ox, oy = (FB_W - w) // 2, (FB_H - h) // 2
if ox < 0 or oy < 0:
    print('image bigger than screen, cropping', flush=True)
rows = []
for y in range(h):
    dy = oy + y
    if dy < 0 or dy >= FB_H:
        continue
    line = bytearray()
    for x in range(w):
        dx = ox + x
        if dx < 0 or dx >= FB_W:
            continue
        r, g, b = px[y * w + x]
        line += bytes((b, g, r, 0))
    base = dy * PITCH + max(ox, 0) * 4
    mm[base:base + len(line)] = line
mm.flush()
print('blitted, pass --hold to keep process alive', flush=True)
if '--hold' in sys.argv:
    import time
    while True:
        time.sleep(60)
