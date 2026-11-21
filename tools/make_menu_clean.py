#!/usr/bin/env python3
"""Make data/menu2_sf.dat: Formido's settings background with the painted panel text removed.

The original settings screen has the key-binding labels painted into the picture. The SF3500 port
draws its own rows (controls, frame limit, credits, music volume) with the game's font, so the panel
interior is cleaned here: pixels that differ from a smooth local background are masked, grown a little
to cover the glow, and filled by diffusion from the surrounding panel.

usage: make_menu_clean.py data/menu2.dat data/menu2_sf.dat   (needs Pillow, numpy, scipy)"""
import sys
import numpy as np
from PIL import Image
from scipy import ndimage as ndi

src, dst = sys.argv[1], sys.argv[2]
im = np.asarray(Image.open(src).convert("RGB")).astype(np.float32)
H, W, _ = im.shape

# panel interior (the box under the logo): keep a margin from its border
x0, x1, y0, y1 = 284, 598, 140, 462
reg = im[y0:y1, x0:x1]
lum = reg.mean(axis=2)
smooth = ndi.median_filter(lum, size=(41, 41), mode="nearest")
mask = np.abs(lum - smooth) > 14
mask = ndi.binary_dilation(mask, iterations=5)

out = reg.copy()
known = ~mask
# diffusion fill: repeatedly replace masked pixels by the mean of their neighbours
fill = reg.copy()
fill[mask] = smooth[mask][:, None]
for _ in range(300):
    blur = ndi.uniform_filter(fill, size=(5, 5, 1), mode="nearest")
    fill[mask] = blur[mask]
out[mask] = fill[mask]

res = im.copy()
res[y0:y1, x0:x1] = out
Image.fromarray(np.clip(res + 0.5, 0, 255).astype(np.uint8)).save(dst, format="PNG")
print("wrote", dst, "masked", int(mask.sum()), "px")
