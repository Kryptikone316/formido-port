#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Collision stress test for the enhanced game's cross-shaped world.

Reproduces the game's own movement rules (PLAYER::move in src/player.cpp, enhanced_unstick in src/enhanced.cpp) on the
real collision maps (data/collisionmap_enh.dat and data/waste_?_col.dat), then shoves a simulated player into every kind
of wall, corner and crack with random impulses and holds, and counts:

  tunnel    the player ended up on the far side of a wall, in free space that is not connected to where it started
            (or outside the cross)
  embedded  the player was left inside solid rock after a move (the game then relies on enhanced_unstick)
  teleport  enhanced_unstick had to move the player more than 12 pixels
  hang      after the shoves, none of the 8 directions lets the player get 25 pixels away within 30 ticks
  pocket    free points whose 8 neighbours at 3 pixels are all blocked (no way out for a point mover)

usage: collision_sim.py <data folder> [--substep] [--runs N]
  --fast      extreme impulses (14 to 32 px per tick) to probe tunnelling through thin walls
  --substep   move in steps of at most 2 pixels (the fix for tunnelling) instead of one step per tick
"""
import sys
import math
import random
import numpy as np
from PIL import Image
from scipy import ndimage as ndi

W, H = 640, 480
CELLS = {(0, 0): "ground", (0, -1): "n", (1, 0): "e", (0, 1): "s", (-1, 0): "w"}


def load_world(d):
    world = np.ones((3 * H, 3 * W), bool)          # True = blocked; everything outside the cross is rock
    for (cx, cy), name in CELLS.items():
        f = "collisionmap_enh.dat" if name == "ground" else f"waste_{name}_col.dat"
        im = np.asarray(Image.open(f"{d}/{f}").convert("RGB")).astype(int)
        solid = ((im[..., 0] == 255) & (im[..., 1] == 255) & (im[..., 2] == 255)) | \
                ((im[..., 0] == 0) & (im[..., 1] == 0) & (im[..., 2] == 255))      # white and blue block movement
        world[(cy + 1) * H:(cy + 2) * H, (cx + 1) * W:(cx + 2) * W] = solid
    return world


class Sim:
    def __init__(self, world, substep):
        self.w = world
        self.substep = substep
        self.contact = False

    def blocked(self, x, y):
        xi, yi = int(math.floor(x)) + W, int(math.floor(y)) + H     # world origin: the centre screen's top left is (0, 0)
        if xi < 0 or yi < 0 or xi >= 3 * W or yi >= 3 * H:
            return True
        return bool(self.w[yi, xi])

    def open_spot(self, x, y):
        return not (self.blocked(x, y) or self.blocked(x + 5, y) or self.blocked(x - 5, y) or
                    self.blocked(x, y + 5) or self.blocked(x, y - 5))

    def unstick(self, x, y):
        """enhanced_unstick: spiral out to the nearest open spot (up to 100 px)."""
        if not self.blocked(x, y):
            return x, y, 0.0
        for r in range(2, 101, 2):
            for k in range(24):
                a = k * 2 * math.pi / 24
                nx, ny = x + int(math.cos(a) * r), y + int(math.sin(a) * r)
                if self.open_spot(nx, ny):
                    return float(nx), float(ny), float(r)
        return x, y, 999.0

    def tick(self, p, mx, my, speed=1.0):
        """One game tick of PLAYER::move (twin-stick preset) for the player state p = [x, y, dx, dy]."""
        x, y, dx, dy = p
        if mx or my:
            l = (0.7071 if (mx and my) else 1.0) * speed
            dx += mx * 2 * l
            dy += my * 2 * l
        sx, sy = x, y
        if self.substep:
            n = max(1, int(math.ceil(max(abs(dx), abs(dy)) / 2.0)))
            ax, ay = dx / n, dy / n
            for _ in range(n):
                if not self.blocked(x + ax, y):
                    x += ax
                else:
                    dx = 0.0
                    ax = 0.0
                if not self.blocked(x, y + ay):
                    y += ay
                else:
                    dy = 0.0
                    ay = 0.0
        else:
            x += dx
            if self.blocked(x, y):
                x = sx
            y += dy
            if self.blocked(x, y):
                y = sy
        if self.contact:
            # PLAYER::col_critter, with a bug overlapping the player: step back and move only 30 % of the velocity
            nx, ny = sx + dx * 0.3, sy + dy * 0.3
            if self.blocked(nx, ny):
                nx, ny = sx, sy
            x, y = nx, ny
            dx += random.uniform(-1, 1)
            dy += random.uniform(-1, 1)
        dx *= 0.7
        dy *= 0.7
        p[0], p[1], p[2], p[3] = x, y, dx, dy


FAST = [14.0, 18.0, 24.0, 32.0]      # far beyond anything in the game (the strongest knock-back is about 12)
DIRS = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)]


def main():
    d = sys.argv[1]
    substep = "--substep" in sys.argv
    runs = 6000
    if "--runs" in sys.argv:
        runs = int(sys.argv[sys.argv.index("--runs") + 1])
    random.seed(7)
    world = load_world(d)
    sim = Sim(world, substep)

    free = ~world
    lab, n = ndi.label(free)                      # 4-connected free components (a point mover cannot cross a diagonal gap)
    print(f"free components: {n}  (sizes: {sorted(np.bincount(lab.ravel())[1:], reverse=True)[:6]})")

    # pockets: free points with every 3-pixel neighbour blocked
    pk = 0
    ys, xs = np.nonzero(free)
    for y, x in zip(ys[::7], xs[::7]):
        if all(sim.blocked(x - W + dx * 3, y - H + dy * 3) for dx, dy in DIRS):
            pk += 1
    print(f"pocket points (sampled 1 in 7): {pk}")

    # starting points near solids: free pixels within 14 px of a wall, inside the cross
    dist = ndi.distance_transform_edt(free)
    near = np.argwhere(free & (dist <= 14))
    stats = dict(tunnel=0, embedded=0, teleport=0, hang=0, unstick_calls=0, max_teleport=0.0)
    worst = []
    for run in range(runs):
        yi, xi = near[random.randrange(len(near))]
        x, y = float(xi - W), float(yi - H)
        comp = lab[yi, xi]
        p = [x, y, 0.0, 0.0]
        # the shove: a knock-back impulse (blast, stomp, charge, burst: up to 8-12 px per tick) and a held direction
        ang = random.uniform(0, 2 * math.pi)
        imp = random.choice(FAST if "--fast" in sys.argv else [6.0, 8.0, 10.0, 12.0])
        p[2], p[3] = math.cos(ang) * imp, math.sin(ang) * imp
        hold = random.choice(DIRS)
        speed = random.choice([1.0, 1.0, 1.2])
        for t in range(90):
            if t == 45:
                hold = random.choice(DIRS)
            sim.tick(p, hold[0], hold[1], speed)
            if sim.blocked(p[0], p[1]):
                stats["embedded"] += 1
                nx, ny, r = sim.unstick(p[0], p[1])
                stats["unstick_calls"] += 1
                if r > 12:
                    stats["teleport"] += 1
                    stats["max_teleport"] = max(stats["max_teleport"], r)
                p[0], p[1], p[2], p[3] = nx, ny, 0.0, 0.0
            # the same free component as the start?
            xi2, yi2 = int(math.floor(p[0])) + W, int(math.floor(p[1])) + H
            if 0 <= xi2 < 3 * W and 0 <= yi2 < 3 * H:
                if free[yi2, xi2] and lab[yi2, xi2] != comp:
                    stats["tunnel"] += 1
                    if len(worst) < 5:
                        worst.append((x, y, p[0], p[1], imp))
                    break
            else:
                stats["tunnel"] += 1
                break
        else:
            # can the player get away from here? try each direction for 30 ticks
            ok = False
            for dr in DIRS:
                q = list(p)
                for _ in range(30):
                    sim.tick(q, dr[0], dr[1], speed)
                if math.hypot(q[0] - p[0], q[1] - p[1]) >= 25:
                    ok = True
                    break
            if not ok:
                stats["hang"] += 1
                if len(worst) < 8:
                    worst.append(("hang", p[0], p[1]))
    # how long does it take to get 25 px away from a wall, free vs with a bug in contact (the game's 30 % rule)?
    for contact in (False, True):
        sim.contact = contact
        times, fails = [], 0
        for _ in range(1500):
            yi, xi = near[random.randrange(len(near))]
            best = None
            for dr in random.sample(DIRS, 8):
                q = [float(xi - W), float(yi - H), 0.0, 0.0]
                x0, y0 = q[0], q[1]
                for t in range(240):
                    sim.tick(q, dr[0], dr[1])
                    if math.hypot(q[0] - x0, q[1] - y0) >= 25:
                        best = t if best is None else min(best, t)
                        break
            if best is None:
                fails += 1
            else:
                times.append(best)
        times.sort()
        med = times[len(times) // 2] if times else -1
        p90 = times[int(len(times) * 0.9)] if times else -1
        print(f"escape 25 px (best of 8 directions) {'WITH a bug in contact' if contact else 'free              '}: median {med} ticks, 90th pct {p90} ticks, failures {fails}/1500")
    sim.contact = False
    print(f"runs {runs}  substep {substep}")
    for k, v in stats.items():
        print(f"  {k}: {v}")
    for w in worst:
        print("  example:", w)


if __name__ == "__main__":
    main()
