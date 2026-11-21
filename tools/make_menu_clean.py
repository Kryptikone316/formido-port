#!/usr/bin/env python3
"""Make data/menu2_sf.dat and data/menu_sf.dat: Formido's menu backgrounds without their painted labels.

The settings screen (menu2) has the key-binding labels painted into its panel; the main menu (menu) has the
four menu entries painted into the picture. The SF3500 port draws its own text (more rows, new entries), so the
painted text is removed here: pixels that differ from a smooth local background are masked, grown a little to
cover the glow, and filled by diffusion from the surroundings.

usage: make_menu_clean.py <data folder>    (needs Pillow, numpy, scipy)"""
import os
import sys
import numpy as np
from PIL import Image
from scipy import ndimage as ndi


def clean(src, dst, box, thr, grow, med=41, one_sided=False, bands=None):
    im = np.asarray(Image.open(src).convert("RGB")).astype(np.float32)
    x0, x1, y0, y1 = box
    reg = im[y0:y1, x0:x1]
    lum = reg.mean(axis=2)
    smooth = ndi.median_filter(lum, size=(med, med), mode="nearest")
    diff = (lum - smooth) if one_sided else np.abs(lum - smooth)
    mask = diff > thr
    if bands:
        keep = np.zeros_like(mask)
        for by0, by1, bx0, bx1 in bands:
            keep[by0 - y0:by1 - y0, bx0 - x0:bx1 - x0] = True
        mask &= keep
    mask = ndi.binary_dilation(mask, iterations=grow)
    fill = reg.copy()
    fill[mask] = smooth[mask][:, None]
    for _ in range(300):
        blur = ndi.uniform_filter(fill, size=(5, 5, 1), mode="nearest")
        fill[mask] = blur[mask]
    out = im.copy()
    out[y0:y1, x0:x1][mask] = fill[mask]
    Image.fromarray(np.clip(out + 0.5, 0, 255).astype(np.uint8)).save(dst, format="PNG")
    print("wrote", dst, "masked", int(mask.sum()), "px")


d = sys.argv[1]
clean(os.path.join(d, "menu2.dat"), os.path.join(d, "menu2_sf.dat"), (284, 598, 140, 462), 14, 5)
clean(os.path.join(d, "menu.dat"), os.path.join(d, "menu_sf.dat"), (30, 270, 225, 400), 24, 7, 41, True,
      bands=[(240, 272, 45, 235), (279, 308, 45, 235), (311, 347, 45, 245), (352, 384, 45, 245)])
