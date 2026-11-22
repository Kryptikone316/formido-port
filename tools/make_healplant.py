#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Make data/plant_green.dat: the healing plant of the enhanced game, four frames side by side (80 x 80 each,
transparent background): healthy, yellowing, browning, withered. The game picks the frame from how much is left
of the plant. Same drawing as the red thorn plants of make_wasteland.py, in green.

usage: make_healplant.py data/plant_green.dat      (needs Pillow, numpy, scipy)"""
import os
import sys
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_wasteland import draw_plant

FR = 80
FRAMES = [
    (1.00, (52, 100, 46), lambda c: (int(c * 0.35), int(c * 1.10), int(c * 0.42))),     # healthy
    (0.94, (84, 100, 40), lambda c: (int(c * 0.80), int(c * 1.00), int(c * 0.22))),     # yellowing
    (0.78, (96, 74, 38), lambda c: (int(c * 0.78), int(c * 0.56), int(c * 0.20))),      # browning
    (0.52, (92, 66, 36), lambda c: (int(c * 0.52), int(c * 0.38), int(c * 0.20))),      # withered
]


def main():
    out = sys.argv[1]
    sheet = Image.new("RGBA", (FR * len(FRAMES), FR), (0, 0, 0, 0))
    for i, (scale, stalk, petal) in enumerate(FRAMES):
        big = Image.new("RGBA", (FR * 2, FR * 2), (0, 0, 0, 0))
        rng = np.random.default_rng(5)          # the same plant in every frame
        # a soft shadow on the ground, then the plant standing on it
        d = ImageDraw.Draw(big)
        d.ellipse((FR * 2 * 0.30, FR * 2 * 0.80, FR * 2 * 0.74, FR * 2 * 0.93), fill=(0, 0, 0, 70))
        draw_plant(ImageDraw.Draw(big), rng, FR * 2 * 0.52, FR * 2 * 0.84, 27 * 2 * scale, stalk=stalk, petal=petal)
        sheet.paste(big.resize((FR, FR), Image.LANCZOS), (i * FR, 0))
    sheet.save(out, format="PNG")
    print("wrote", out, sheet.size)


if __name__ == "__main__":
    main()
