#!/usr/bin/env python3
"""Generate a 720x1600 splash PNG for fogos framebuffer demo."""
from PIL import Image, ImageDraw, ImageFont

W, H = 720, 1600
img = Image.new('RGB', (W, H))
px = img.load()
top = (13, 27, 42)
bot = (26, 58, 92)
for y in range(H):
    t = y / (H - 1)
    pxcol = tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3))
    for x in range(W):
        px[x, y] = pxcol

d = ImageDraw.Draw(img, 'RGBA')
# subtle dot grid
for gy in range(80, H - 200, 64):
    for gx in range(40, W, 64):
        d.ellipse([gx - 2, gy - 2, gx + 2, gy + 2], fill=(255, 255, 255, 18))

cx, cy = W // 2, 560
GREEN = (0, 153, 0)
# logo ring + inner disc + needle (pinecone-ish abstract)
d.ellipse([cx - 150, cy - 150, cx + 150, cy + 150], outline=(255, 255, 255), width=10)
d.ellipse([cx - 96, cy - 96, cx + 96, cy + 96], outline=GREEN + (255,), width=8)
d.ellipse([cx - 30, cy - 30, cx + 30, cy + 30], fill=(255, 255, 255))
d.line([cx, cy + 96, cx, cy + 150], fill=GREEN + (255,), width=8)

fb = '/usr/share/fonts/TTF/DejaVuSans-Bold.ttf'
fr = '/usr/share/fonts/TTF/DejaVuSans.ttf'
f_title = ImageFont.truetype(fb, 96)
f_sub = ImageFont.truetype(fb, 44)
f_small = ImageFont.truetype(fr, 32)

def center(y, text, font, fill=(255, 255, 255)):
    bb = d.textbbox((0, 0), text, font=font)
    d.text(((W - (bb[2] - bb[0])) / 2 - bb[0], y), text, font=font, fill=fill)

center(830, 'Nura OS', f_title)
center(950, 'Moto G34 5G  \u2022  fogos', f_sub, (0, 200, 0))
center(1120, 'postmarketOS  \u2022  5.4.302-moto', f_small, (170, 190, 210))
center(1170, 'drm + fbcon  \u2022  720x1600', f_small, (170, 190, 210))
center(1480, 'touch: modprobe ilitek_v3_mmi', f_small, (120, 140, 160))

img.save('/home/princeofgiri/nura-fogos/tmp/splash-fogos-720x1600.png')
print('saved, size:', img.size)
