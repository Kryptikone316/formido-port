#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Make the four wasteland screens of Formido's enhanced game (north, east, south, west arms of the map).

Output (into the given folder, 640x480 PNG files with a .dat name, like the game's own data):
    waste_n.dat waste_e.dat waste_s.dat waste_w.dat      pictures
    waste_n_col.dat ...                                    collision maps (white = solid, red = thorns, magenta = free)

How the pictures are made, in the spirit of the concept art (rough pebbled sand, dark warm-brown rock, red alien plants,
dust in the air):
  1. the original ground.dat with its platform and pylons removed: the hole is filled with the smooth colour of the
     surroundings plus ripple detail taken from clean sand, so the picture keeps the original's own light and ripples;
  2. each arm uses that sand flipped differently, so the four do not look alike;
  3. rocky slopes grow out of the three closed sides (the side facing the centre screen stays open sand);
  4. crags stand on the ground as obstacles (white in the collision map), with a cast shadow;
  5. cliff walls close the three outer sides (solid);
  6. pebbles, red plants and a little dust finish it.
Deterministic (fixed seeds).

usage: make_wasteland.py data/ground.dat out_folder     (needs Pillow, numpy, scipy)"""
import os
import sys
import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as ndi

W, H = 640, 480
LIGHT = np.array([-0.62, -0.78])      # light from the top left; shadows fall to the bottom right


def fbm(rng, shape, scales, weights):
    """Sum of smooth noise layers (value noise, cubic upsampled), zero-centred."""
    out = np.zeros(shape, np.float32)
    for sc, wt in zip(scales, weights):
        gh, gw = max(2, int(shape[0] / sc) + 2), max(2, int(shape[1] / sc) + 2)
        g = rng.random((gh, gw)).astype(np.float32)
        up = ndi.zoom(g, (shape[0] / (gh - 1), shape[1] / (gw - 1)), order=3)[: shape[0], : shape[1]]
        if up.shape != shape:
            up = np.pad(up, ((0, shape[0] - up.shape[0]), (0, shape[1] - up.shape[1])), mode="edge")
        out += wt * (up - 0.5)
    return out


def light(height, strength):
    """Directional shading of a height field: 1.0 = flat."""
    gy, gx = np.gradient(ndi.gaussian_filter(height, 0.7))
    return np.clip(1.0 + strength * (gx * LIGHT[0] + gy * LIGHT[1]), 0.35, 1.7)


def clean_sand(ground, rng):
    """ground.dat without the platform and pylons."""
    hole = np.zeros((H, W), bool)
    hole[95:385, 200:440] = True
    for cx, cy in ((320, 78), (160, 240), (478, 240), (320, 400)):
        hole[cy - 52:cy + 58, cx - 56:cx + 62] = True
    hole[130:420, 215:470] = True        # platform shadow
    hole[50:140, 300:400] = True
    hole[200:340, 440:560] = True
    hole[360:470, 290:400] = True
    hole[210:350, 100:230] = True
    hole = ndi.binary_dilation(hole, iterations=6)
    keep = ~hole

    # low frequency: diffuse the surrounding colour into the hole
    low = ndi.gaussian_filter(ground, (8, 8, 0))
    fill = low.copy()
    fill[hole] = low[keep].mean(axis=0)
    for _ in range(600):
        fill[hole] = ndi.uniform_filter(fill, size=(9, 9, 1), mode="nearest")[hole]

    # detail: ripples copied from clean sand in random patches
    detail = ground - ndi.gaussian_filter(ground, (4, 4, 0))
    ok = ndi.binary_erosion(keep, iterations=32)
    ys, xs = np.nonzero(ok)
    out = np.zeros_like(ground)
    wsum = np.zeros((H, W), np.float32)
    patch, ov = 48, 16
    ramp = np.minimum(np.arange(patch), np.arange(patch)[::-1]).astype(np.float32) + 1.0
    win = np.minimum(np.minimum(ramp[:, None], ramp[None, :]) / ov, 1.0)
    for y in range(95 - patch, 400, patch - ov):
        for x in range(60 - patch, 560, patch - ov):
            k = rng.integers(len(ys))
            sy, sx = ys[k] - patch // 2, xs[k] - patch // 2
            src = detail[sy:sy + patch, sx:sx + patch]
            y0, x0, y1, x1 = max(y, 0), max(x, 0), min(y + patch, H), min(x + patch, W)
            if y1 <= y0 or x1 <= x0:
                continue
            ww = win[y0 - y:y1 - y, x0 - x:x1 - x]
            out[y0:y1, x0:x1] += src[y0 - y:y1 - y, x0 - x:x1 - x] * ww[..., None]
            wsum[y0:y1, x0:x1] += ww
    patched = out / np.maximum(wsum, 1e-3)[..., None]
    synth = fill + patched

    feather = ndi.gaussian_filter(hole.astype(np.float32), 5.0)[..., None]
    res = ground * (1 - feather) + synth * feather
    return np.clip(res, 0, 255)


def blob_polygon(rng, cx, cy, r):
    n = int(rng.integers(9, 14))
    ang = np.sort(rng.random(n) * 2 * np.pi)
    rad = r * (0.66 + 0.5 * rng.random(n))
    rad[::2] *= 0.82
    return [(cx + rr * np.cos(a), cy + rr * np.sin(a) * 0.84) for a, rr in zip(ang, rad)]


def raster(poly):
    img = Image.new("L", (W, H), 0)
    ImageDraw.Draw(img).polygon(poly, fill=255)
    return np.asarray(img) > 0


def draw_plant(draw, rng, cx, cy, size, stalk=(104, 46, 30), petal=lambda c: (int(c * 1.15), int(c * 0.2), int(c * 0.13))):
    """A red spiky alien flower: stalks ending in a burst of long, curled petals."""
    S = size
    for _ in range(int(rng.integers(1, 3))):
        bx, by = cx + rng.normal(0, S * 0.25), cy + rng.normal(0, S * 0.25)
        tx, ty = bx + rng.normal(0, S * 0.3), by - S * (0.5 + 0.5 * rng.random())
        pts = [(bx + (tx - bx) * t + np.sin(t * 3) * 3, by + (ty - by) * t) for t in np.linspace(0, 1, 8)]
        draw.line(pts, fill=stalk + (255,), width=max(2, int(S * 0.09)))
        n = int(rng.integers(6, 10))
        for k in range(n):
            a = -np.pi / 2 + (k - (n - 1) / 2) * (np.pi * 1.3 / n) + rng.normal(0, 0.12)
            ln = S * (0.7 + 0.5 * rng.random())
            curl = rng.normal(0, 0.7)
            px, py = tx, ty
            for step in range(9):
                a2 = a + curl * step / 9.0
                nx, ny = px + np.cos(a2) * ln / 9, py + np.sin(a2) * ln / 9 + step * 0.3
                c = 150 + 70 * (step / 9.0)
                draw.line([(px, py), (nx, ny)], fill=petal(c) + (255,),
                          width=max(1, int(S * 0.11 * (1 - step / 10.0))))
                px, py = nx, ny


BODY = 19


def clean_collision(solid, inward):
    """Make the collision map easy to walk: no thin rock spikes, no tiny pockets or narrow fingers a body would
    wedge in, nothing free that cannot be reached from the open side."""
    solid = ndi.binary_opening(solid, iterations=2)
    free = ~solid
    # places where a body of radius BODY can stand: the player is about 40 pixels across and the bugs up to 56, so a gap
    # narrower than that is a trap (slots, cracks and dead-end gorges along the cliffs); it is filled in as rock
    core = ndi.binary_erosion(free, iterations=BODY)
    lab, n = ndi.label(core)
    edge = np.zeros((H, W), bool)
    if inward == "bottom":
        edge[H - 40:, :] = True
    elif inward == "top":
        edge[:40, :] = True
    elif inward == "left":
        edge[:, :40] = True
    else:
        edge[:, W - 40:] = True
    keep_labels = np.unique(lab[edge & (lab > 0)])
    keep = np.isin(lab, keep_labels) & (lab > 0)
    reach = ndi.binary_dilation(keep, iterations=BODY) & free
    return ~reach


def make_arm(name, sand, seed):
    rng = np.random.default_rng(seed)
    inward = {"n": "bottom", "s": "top", "e": "left", "w": "right"}[name]

    # 2. flip the sand differently per arm
    col = sand.copy()
    if name in ("e", "s"):
        col = col[:, ::-1]
    if name in ("s", "w"):
        col = col[::-1, :]
    col = np.ascontiguousarray(col)

    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    # distance from the open side, 0..1
    if inward == "bottom":
        away = (H - 1 - yy) / H
    elif inward == "top":
        away = yy / H
    elif inward == "left":
        away = xx / W
    else:
        away = (W - 1 - xx) / W

    # 3. rocky slopes, growing towards the closed sides
    n1 = ndi.gaussian_filter(fbm(rng, (H, W), (120, 60, 30), (1.0, 0.6, 0.3)), 3)
    rockiness = np.clip((away - 0.5 + n1 * 0.9) * 7.0, 0.0, 1.0)
    relief = fbm(rng, (H, W), (42, 20, 10, 5), (1.0, 0.7, 0.4, 0.2))
    rock_lit = light(relief * 14.0, 0.45)
    base = 0.5 + 0.7 * np.clip(relief * 1.4 + 0.5, 0, 1)
    rock_col = np.stack([base * 82, base * 52, base * 38], axis=2) * rock_lit[..., None]
    col = col * (1 - rockiness[..., None]) + rock_col * rockiness[..., None]

    # keep the open side and a corridor clear of crags
    keep_clear = np.zeros((H, W), bool)
    if inward == "bottom":
        keep_clear[H - 160:, :] = True
    elif inward == "top":
        keep_clear[:160, :] = True
    elif inward == "left":
        keep_clear[:, :190] = True
    else:
        keep_clear[:, W - 190:] = True

    # 4. crags
    # distance of every pixel to the cliff walls (computed from the same band as step 5 below)
    band0 = np.zeros((H, W), np.float32)
    jag0 = ndi.gaussian_filter(fbm(np.random.default_rng(seed + 99), (H, W), (26, 11), (1.0, 0.6)), 0.6) * 56.0
    if inward != "top":
        band0 = np.maximum(band0, 34.0 + jag0 - yy)
    if inward != "bottom":
        band0 = np.maximum(band0, 34.0 + jag0 - (H - 1 - yy))
    if inward != "left":
        band0 = np.maximum(band0, 34.0 + jag0 - xx)
    if inward != "right":
        band0 = np.maximum(band0, 34.0 + jag0 - (W - 1 - xx))
    dcliff = ndi.distance_transform_edt(~(band0 > 0))
    centres = []
    for _ in range(500):
        if len(centres) >= 7:
            break
        r = float(rng.integers(30, 52))
        cx, cy = float(rng.integers(100, W - 100)), float(rng.integers(100, H - 100))
        probe = [(cx, cy), (cx + r, cy), (cx - r, cy), (cx, cy + r), (cx, cy - r)]
        if any(keep_clear[int(min(max(py, 0), H - 1)), int(min(max(px, 0), W - 1))] for px, py in probe):
            continue
        if any((cx - ox) ** 2 + (cy - oy) ** 2 < (r + orr + 84) ** 2 for ox, oy, orr in centres):
            continue
        if dcliff[int(cy), int(cx)] < r + 60:			# leave room to walk between a crag and the cliff
            continue
        centres.append((cx, cy, r))
    height = np.zeros((H, W), np.float32)
    shadow = np.zeros((H, W), np.float32)
    solid = np.zeros((H, W), bool)
    for cx, cy, r in centres:
        m = raster(blob_polygon(rng, cx, cy, r))
        solid |= ndi.binary_erosion(m, iterations=3)
        d = ndi.distance_transform_edt(m).astype(np.float32)
        h = np.minimum(d, 12.0) * 1.6 + d * 0.35
        h += fbm(rng, (H, W), (16, 8), (1.0, 0.6)) * 7.0 * m
        # facets: a few flat planes with their own slope and tone, like broken stone
        ys_, xs_ = np.nonzero(m)
        seeds = np.stack([cx + rng.normal(0, r * 0.45, 6), cy + rng.normal(0, r * 0.4, 6)], axis=1)
        dist = (xs_[:, None] - seeds[None, :, 0]) ** 2 + (ys_[:, None] - seeds[None, :, 1]) ** 2
        which = dist.argmin(axis=1)
        tilt = rng.normal(0, 0.55, (6, 2))
        plane = tilt[which, 0] * (xs_ - cx) * 0.5 + tilt[which, 1] * (ys_ - cy) * 0.5
        tone = rng.normal(0, 3.0, 6)[which]
        h2 = h.copy()
        h2[ys_, xs_] += plane + tone
        height = np.maximum(height, h2 * m)
        mm = m.astype(np.float32)
        shadow = np.maximum(shadow, np.roll(np.roll(mm, 8, axis=0), 11, axis=1) * 0.6)
        shadow = np.maximum(shadow, np.roll(np.roll(mm, 15, axis=0), 21, axis=1) * 0.35)
    crag = height > 0
    sh = ndi.gaussian_filter(shadow * (~crag), 2.4)
    col *= (1.0 - 0.55 * sh)[..., None]
    cl = light(height, 0.55)
    top = np.clip(height / 26.0, 0, 1)
    cb = 0.62 + 0.62 * top
    crag_col = np.stack([cb * 98, cb * 62, cb * 46], axis=2) * cl[..., None]
    rim = ndi.binary_dilation(crag, iterations=2) & ~ndi.binary_erosion(crag, iterations=2)
    crag_col *= np.where(rim, 0.6, 1.0)[..., None]
    col[crag] = crag_col[crag]

    # 5. cliff walls on the closed sides
    band = np.zeros((H, W), np.float32)
    jag = ndi.gaussian_filter(fbm(rng, (H, W), (26, 11), (1.0, 0.6)), 0.6) * 56.0
    depth = 34.0
    if inward != "top":
        band = np.maximum(band, depth + jag - yy)
    if inward != "bottom":
        band = np.maximum(band, depth + jag - (H - 1 - yy))
    if inward != "left":
        band = np.maximum(band, depth + jag - xx)
    if inward != "right":
        band = np.maximum(band, depth + jag - (W - 1 - xx))
    band = np.minimum(band, 72.0)           # never deeper than the corner blocks of the centre screen
    cliff = band > 0
    solid |= cliff
    solid = clean_collision(solid, inward)
    # whatever the cleanup closed (slots, cracks, pockets) is drawn as cliff rock too, so picture and collision agree
    extra = solid & ~cliff & ~ndi.binary_dilation(crag, iterations=3)
    cliff = cliff | extra
    band = np.where(extra, np.maximum(band, 10.0), band)
    ch = np.clip(band, 0, 70)
    cll = light(ch * 0.9 + fbm(rng, (H, W), (14, 7), (1.0, 0.7)) * 10.0, 0.22)
    cbse = 0.45 + ch / 120.0
    cliff_col = np.stack([cbse * 90, cbse * 57, cbse * 42], axis=2) * cll[..., None]
    edge = ndi.binary_dilation(cliff, iterations=4) & ~cliff
    col = np.where(cliff[..., None], cliff_col, col)
    col *= np.where(edge, 0.72, 1.0)[..., None]

    # 6a. pebbles
    pebbles = np.zeros((H, W), np.float32)
    pshadow = np.zeros((H, W), np.float32)
    for _ in range(90):
        px, py = int(rng.integers(8, W - 8)), int(rng.integers(8, H - 8))
        if crag[py, px] or cliff[py, px]:
            continue
        r = int(rng.integers(2, 5))
        yy2, xx2 = np.ogrid[-r:r + 1, -r:r + 1]
        disc = (xx2 ** 2 + (yy2 * 1.2) ** 2 <= r * r).astype(np.float32)
        pebbles[py - r:py + r + 1, px - r:px + r + 1] = np.maximum(pebbles[py - r:py + r + 1, px - r:px + r + 1], disc)
        pshadow[py - r + 2:py + r + 3, px - r + 3:px + r + 4] = np.maximum(
            pshadow[py - r + 2:py + r + 3, px - r + 3:px + r + 4], disc * 0.55)
    pshadow = ndi.gaussian_filter(pshadow, 1.0)
    col *= (1 - 0.5 * pshadow * (1 - pebbles))[..., None]
    pm = pebbles > 0.4
    peb = np.array([112, 74, 52], np.float32) * (0.85 + 0.35 * light(ndi.gaussian_filter(pebbles, 0.8) * 6, 0.5))[..., None]
    col[pm] = peb[pm]

    # 6b. red plants near rocks and walls (decoration)
    plants = Image.new("RGBA", (W * 2, H * 2), (0, 0, 0, 0))
    pshade = Image.new("RGBA", (W * 2, H * 2), (0, 0, 0, 0))
    spots = []
    for cx, cy, r in centres:
        for _ in range(int(rng.integers(0, 3))):
            a = rng.random() * 2 * np.pi
            spots.append((cx + np.cos(a) * (r + 24), cy + np.sin(a) * (r + 18)))
    for _ in range(int(rng.integers(9, 14))):
        spots.append((float(rng.integers(50, W - 50)), float(rng.integers(60, H - 40))))
    dts = ndi.distance_transform_edt(~solid)
    for sx, sy in spots:
        sx, sy = min(max(sx, 40), W - 40), min(max(sy, 50), H - 24)
        sz = float(rng.integers(20, 32))
        if dts[int(sy), int(sx)] < sz * 1.3 + 4:		# a plant never overlaps or leans on solid rock
            continue
        draw_plant(ImageDraw.Draw(pshade), rng, (sx + 6) * 2, (sy + 4) * 2, sz * 2)
        draw_plant(ImageDraw.Draw(plants), rng, sx * 2, sy * 2, sz * 2)
    shd = np.asarray(pshade.resize((W, H), Image.LANCZOS))[..., 3].astype(np.float32) / 255.0
    col *= (1 - 0.5 * ndi.gaussian_filter(shd, 1.5))[..., None]
    pl = np.asarray(plants.resize((W, H), Image.LANCZOS)).astype(np.float32)
    a = pl[..., 3:4] / 255.0
    col = col * (1 - a) + pl[..., :3] * a
    thorn = ndi.binary_dilation(a[..., 0] > 0.3, iterations=4) & ~solid

    # 6c. dust
    haze = np.clip(ndi.gaussian_filter(fbm(rng, (H, W), (200, 100), (1.0, 0.6)), 10) * 1.4 + 0.2, 0, 0.4)
    col = col * (1 - 0.3 * haze[..., None]) + np.array([150, 96, 40], np.float32) * (0.3 * haze[..., None])
    specks = ndi.gaussian_filter((rng.random((H, W)) > 0.99965).astype(np.float32), 0.7) * 5.0
    col += (np.clip(specks, 0, 1) * 150)[..., None] * np.array([1.0, 0.9, 0.78])

    col = np.clip(col, 0, 255)
    img = Image.fromarray(col.astype(np.uint8), "RGB")
    colmap = np.zeros((H, W, 3), np.uint8)
    colmap[:] = (255, 0, 255)
    colmap[thorn] = (255, 0, 0)			# thorns: walkable, a scratch now and then
    colmap[solid] = (255, 255, 255)
    return img, Image.fromarray(colmap, "RGB"), solid


def main():
    ground_path, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    ground = np.asarray(Image.open(ground_path).convert("RGB")).astype(np.float32)
    sand = clean_sand(ground, np.random.default_rng(7))
    Image.fromarray(sand.astype(np.uint8)).save(os.path.join(out, "sand_clean.png"))
    for i, name in enumerate("nesw"):
        img, colmap, solid = make_arm(name, sand, 1000 + 17 * i)
        img.save(os.path.join(out, f"waste_{name}.dat"), format="PNG")
        colmap.save(os.path.join(out, f"waste_{name}_col.dat"), format="PNG")
        print(name, "solid", round(float(solid.mean()) * 100, 1), "%")


if __name__ == "__main__":
    main()
