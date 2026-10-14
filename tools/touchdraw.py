#!/usr/bin/env python3
"""Live touch visualizer: draws touch points from /dev/input/eventN onto /dev/fb0.
Usage: touchdraw.py [event-dev]  (default /dev/input/event2). Runs until killed."""
import os, mmap, struct, sys, fcntl, time

FB_W, FB_H = 720, 1600
EV_SYN, EV_KEY, EV_ABS = 0x00, 0x01, 0x03
SYN_REPORT = 0
BTN_TOUCH = 0x14A
ABS_X, ABS_Y = 0x00, 0x01
ABS_MT_SLOT = 0x2F
ABS_MT_POSITION_X, ABS_MT_POSITION_Y = 0x35, 0x36
ABS_MT_TRACKING_ID = 0x39
X_MAX, Y_MAX = 2879, 6399
# calibrated active-area margins (5-point + free taps, 18 touches):
# raw extremes X 130..2731, Y 153..6235 map to screen edges
X_MIN, X_SPAN = 130, 2731 - 130
Y_MIN, Y_SPAN = 153, 6235 - 153

def get_pitch():
    fb = os.open('/dev/fb0', os.O_RDONLY)
    fix = fcntl.ioctl(fb, 0x4602, bytes(128))
    os.close(fb)
    return struct.unpack('I', fix[48:52])[0]

PITCH = get_pitch()
fbfd = os.open('/dev/fb0', os.O_RDWR)
mm = mmap.mmap(fbfd, PITCH * FB_H, access=mmap.ACCESS_WRITE)

BG = (10, 20, 35)
GRID = (30, 50, 75)
TOUCH_COLS = [(0, 220, 0), (0, 160, 255), (255, 200, 0), (255, 80, 80),
              (200, 0, 200), (0, 220, 220), (255, 255, 255),
              (140, 255, 140), (255, 170, 0), (150, 150, 255)]

def setpx(x, y, col):
    if 0 <= x < FB_W and 0 <= y < FB_H:
        r, g, b = col
        mm[y * PITCH + x * 4:y * PITCH + x * 4 + 4] = bytes((b, g, r, 0))

def circle(cx, cy, rad, col):
    for y in range(cy - rad, cy + rad + 1):
        for x in range(cx - rad, cx + rad + 1):
            if (x - cx) ** 2 + (y - cy) ** 2 <= rad * rad:
                setpx(x, y, col)

def redraw_bg():
    mm[:] = bytes(BG) * (PITCH * FB_H // 3) + b'\x00' * (PITCH * FB_H % 3)
    # border + grid dots
    for x in range(FB_W):
        for y in (0, 1, 2, FB_H - 3, FB_H - 2, FB_H - 1):
            setpx(x, y, GRID)
    for y in range(FB_H):
        for x in (0, 1, 2, FB_W - 3, FB_W - 2, FB_W - 1):
            setpx(x, y, GRID)
    for gx in range(60, FB_W, 120):
        for gy in range(100, FB_H, 120):
            setpx(gx, gy, GRID)
    # reference crosshair: center axes full-length + corner squares
    AX = (90, 90, 110)
    for x in range(FB_W):
        setpx(x, FB_H // 2, AX)
    for y in range(FB_H):
        setpx(FB_W // 2, y, AX)
    for cx in (40, FB_W - 41):
        for cy in (40, FB_H - 41):
            for dx in range(-18, 19):
                for dy in (-18, 18):
                    setpx(cx + dx, cy + dy, AX)
            for dy in range(-18, 19):
                for dx in (-18, 18):
                    setpx(cx + dx, cy + dy, AX)

R_DOT = 30
clean = None  # snapshot of bg for fast erase

def erase_dot(cx, cy):
    global clean
    x0, x1 = max(0, cx - R_DOT - 2), min(FB_W, cx + R_DOT + 3)
    y0, y1 = max(0, cy - R_DOT - 2), min(FB_H, cy + R_DOT + 3)
    for y in range(y0, y1):
        o = y * PITCH
        mm[o + x0 * 4:o + x1 * 4] = clean[o + x0 * 4:o + x1 * 4]

def dev_xy(ax, ay):
    x = (ax - X_MIN) * FB_W / X_SPAN
    y = (ay - Y_MIN) * FB_H / Y_SPAN
    return max(0, min(FB_W - 1, int(x))), max(0, min(FB_H - 1, int(y)))

redraw_bg()
clean = bytes(mm)
fingers = {}   # slot -> [x, y, down]
drawn = {}     # slot -> (x, y) currently on screen
active = set()
slot = 0
btn = 0
ax = ay = 0
mtx, mty = {}, {}
last_draw = 0.0
pending = False
last_beat = 0.0
beat_on = False
last_tlog = 0.0
plog = open('/tmp/touch-points.log', 'a')
evdev = sys.argv[1] if len(sys.argv) > 1 else '/dev/input/event2'
fd = os.open(evdev, os.O_RDONLY | os.O_NONBLOCK)
print(f'listening on {evdev}, pitch={PITCH}', flush=True)
buf = b''
while True:
    try:
        data = os.read(fd, 24 * 64)
    except BlockingIOError:
        data = b''
    if data:
        buf += data
    if not buf:
        time.sleep(0.005)
        continue
    n = (len(buf) // 24) * 24
    data, buf = buf[:n], buf[n:]
    if not data:
        continue
    dirty = False
    for off in range(0, len(data) - 23, 24):
        sec, usec, typ, code, val = struct.unpack('llHHi', data[off:off + 24])
        if typ == EV_ABS:
            if code == ABS_MT_SLOT:
                slot = val
            elif code == ABS_MT_POSITION_X:
                mtx[slot] = val
            elif code == ABS_MT_POSITION_Y:
                mty[slot] = val
            elif code == ABS_MT_TRACKING_ID:
                if val == -1:
                    fingers.pop(slot, None)
                    active.discard(slot)
                else:
                    active.add(slot)
                    fingers[slot] = [mtx.get(slot, 0), mty.get(slot, 0), True]
                dirty = True
            elif code == ABS_X:
                ax = val
            elif code == ABS_Y:
                ay = val
        elif typ == EV_KEY and code == BTN_TOUCH:
            if val == 0:
                fingers.clear()
            else:
                for s in list(active):
                    fx, fy = mtx.get(s, 0), mty.get(s, 0)
                    dx, dy = dev_xy(fx, fy)
                    plog.write('DOWN slot=%d raw=(%d,%d) mapped=(%d,%d)\n' % (s, fx, fy, dx, dy))
                    plog.flush()
            dirty = True
        elif typ == EV_SYN and code == SYN_REPORT:
            for s in active:
                fingers[s] = [mtx.get(s, 0), mty.get(s, 0), True]
            dirty = True
            now3 = time.monotonic()
            if fingers and now3 - globals().get('last_log', 0) > 1.0:
                globals()['last_log'] = now3
                for s, (fx, fy, _) in sorted(fingers.items()):
                    dx, dy = dev_xy(fx, fy)
                    plog.write('HOLD slot=%d raw=(%d,%d) mapped=(%d,%d)\n' % (s, fx, fy, dx, dy))
                plog.flush()
    if dirty or pending:
        now = time.monotonic()
        if now - last_draw < 0.033:
            pending = True
            continue
        pending = False
        last_draw = now
        for s in list(drawn):
            if s not in fingers:
                erase_dot(*drawn.pop(s))
        for i, (s, (fx, fy, _)) in enumerate(sorted(fingers.items())):
            dx, dy = dev_xy(fx, fy)
            if drawn.get(s) != (dx, dy):
                if s in drawn:
                    erase_dot(*drawn[s])
                circle(dx, dy, R_DOT, TOUCH_COLS[i % len(TOUCH_COLS)])
                circle(dx, dy, 10, (255, 255, 255))
                drawn[s] = (dx, dy)
                now4 = time.monotonic()
                if now4 - last_tlog > 0.1:
                    last_tlog = now4
                    plog.write('DRAW slot=%d raw=(%d,%d) mapped=(%d,%d)\n' % (s, fx, fy, dx, dy))
                    plog.flush()
        mm.flush()
    # heartbeat: blink top-left corner square every 2s so user sees app alive
    now2 = time.monotonic()
    if now2 - last_beat > 2.0:
        last_beat = now2
        beat_on = not beat_on
        c = (0, 255, 0) if beat_on else GRID
        for dx in range(-18, 19):
            for dy in (-18, 18):
                setpx(40 + dx, 40 + dy, c)
        for dy in range(-18, 19):
            for dx in (-18, 18):
                setpx(40 + dx, 40 + dy, c)
        mm.flush()
