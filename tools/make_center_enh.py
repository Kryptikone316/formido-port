#!/usr/bin/env python3
"""Make the centre screen of the enhanced game: data/ground_enh.dat and data/collisionmap_enh.dat.

The arms of the cross have cliff walls along their sides, right up to the edge they share with the centre screen. That left an
invisible barrier along the first stretch of each edge of the centre screen, next to its corners. Here the four corners of the
centre screen get rock blocks (same look as the cliffs of the arms) and the collision map treats them as solid, so the boundary
is visible and lines up with the arms: the open way into each arm is the middle 480 pixels of the edge.

usage: make_center_enh.py data     (reads ground.dat and collisionmap.dat there; needs Pillow, numpy, scipy)"""
import os
import sys
import numpy as np
from PIL import Image
from scipy import ndimage as ndi
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_wasteland import fbm, light, W, H

BLOCK = 72          # the open way into an arm is x (or y) in BLOCK .. size - BLOCK (was 80; 10 % smaller for the device screen)


def main():
    d = sys.argv[1]
    ground = np.asarray(Image.open(os.path.join(d, "ground.dat")).convert("RGB")).astype(np.float32)
    col = np.asarray(Image.open(os.path.join(d, "collisionmap.dat")).convert("RGB")).copy()
    rng = np.random.default_rng(4242)

    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    # distance from the nearest corner, as the larger of the two offsets (a square block), with a ragged edge
    dx = np.minimum(xx, W - 1 - xx)
    dy = np.minimum(yy, H - 1 - yy)
    jag = ndi.gaussian_filter(fbm(rng, (H, W), (22, 9), (1.0, 0.6)), 0.6) * 22.0
    corner = np.maximum(dx, dy)
    depth = (BLOCK - 4) + jag - corner
    # only inside the corner squares
    block = (depth > 0) & (dx < BLOCK) & (dy < BLOCK)
    block &= (np.minimum(dx, dy) < BLOCK - 2)

    ch = np.clip(depth, 0, 70)
    cll = light(ch * 0.9 + fbm(rng, (H, W), (14, 7), (1.0, 0.7)) * 10.0, 0.22)
    cbse = 0.45 + ch / 120.0
    rock = np.stack([cbse * 90, cbse * 57, cbse * 42], axis=2) * cll[..., None]
    edge = ndi.binary_dilation(block, iterations=4) & ~block
    out = np.where(block[..., None], rock, ground)
    out *= np.where(edge, 0.72, 1.0)[..., None]
    Image.fromarray(np.clip(out, 0, 255).astype(np.uint8)).save(os.path.join(d, "ground_enh.dat"), format="PNG")

    solid = ndi.binary_erosion(block, iterations=2)
    col[solid] = (255, 255, 255)
    Image.fromarray(col).save(os.path.join(d, "collisionmap_enh.dat"), format="PNG")
    print("corner blocks:", int(block.sum()), "px")


if __name__ == "__main__":
    main()
