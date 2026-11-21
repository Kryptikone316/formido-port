# Formido for the Datafrog SF3500

[Formido](https://mhgames.org/formido/) 1.0.1 by Mika Halttunen (GPL v2 or later), ported to the
Datafrog SF3500 handheld (MIPS32r2, SDL 1.2, no audio device). Upstream sources are in `src/` and
`include/`; every change is under `#ifdef SF3500`. The device glue is in `sf3500/`.

## Controls

Menus: D-pad chooses, A or Start selects, B or Select goes back.

In a match (default, `controls = 1` in `formido.cfg`): twin-stick style. These handhelds' right stick is wired to
the A/B/X/Y buttons, so holding **L1** turns them into the aiming stick.

| Buttons | Action |
|---|---|
| D-pad (left stick) | walk in the direction pressed (8 ways); you face the way you walk |
| L1 + X / A / B / Y | aim and fire up / right / down / left; two face buttons together aim diagonally; you can walk one way and fire another |
| A (without L1) | fire along the current aim |
| R1 | fire along the current aim |
| B (without L1) | special (throw the power-up you hold, in the aim direction) |
| Start | pause |
| Select | end the match |
| Select + Start | leave the game from anywhere |

A press keeps the role it started with: a face button pressed while L1 is held stays an aim button until released,
even if L1 is let go first (and the other way round), so easing off L1 never causes a stray throw.

`controls = 0` restores the original tank controls: Left/Right turn, Up/Down walk along the facing, A shoots,
B is special (the face buttons then ignore L1).

High-score name: Up/Down change the letter, Right takes it, Left or B steps back, A finishes.

Note: the sprite's gun hangs 60 degrees left of the body and bullets leave along `angle - 60`; twin-stick mode keeps a
separate `aim` and sets `angle = aim + 60`, so the picture and the bullets agree.

## The enhanced game

The main menu has **play classic game** (the original, untouched) and **play enhanced game**.

- **A cross-shaped map.** The stone platform screen is in the middle; walking off any side takes you to a wasteland
  screen to the north, east, south or west (rocky ground, crags that block movement and bullets, cliffs on the
  three outer sides). Bugs keep chasing across the edges. A small map in the corner shows where you are and tints a
  screen red when bugs are in it. The wasteland pictures come from `tools/make_wasteland.py`.
- **Power cells.** Each arm holds one power cell, sealed by guardians: a warden (a brute) with escorts, themed per arm
  (north: strikers and runners, east: drones and runners, south: drones and strikers, west: a swarm of runners).
  They stand watch, wake when you come near or hurt one, and walk home and heal if you leave. Defeat them all to free
  the cell (a ring of light marks a sealed cell, and the map shows its dot hollow). Carry it to the stone platform:
  it sets what the pylons offer, one mode at a time (a new cell switches the mode for 2 minutes, the same cell again
  adds 90 seconds, up to 4):
  north = the north pylon becomes an automatic turret, east = bombs and mines are dropped on the platform,
  south = a medbay (the platform heals fast), west = a slowing field around the platform.
  Carrying slows you a little. After a delivery the cell comes back after 100 to 160 seconds, in a new place, with
  fresh guardians that grow stronger the longer the match has gone on. How strong a garrison is changes every time
  (fewer or more guardians, tougher or weaker, quicker or slower); the map shows one to three pips under each arm's
  dot, so you can pick the easier cell, or come back later when the guard has changed. Individual bugs vary too:
  every bug's health and speed are rolled within a range, so no two are the same.
- **Bug ranks.** Drone (the classic bug), runner (small, fast, in packs), striker (tough, hits harder), brute (huge,
  slow). They are the classic bug's pictures scaled and recoloured. They appear as the match goes on.
- **The hive adapts.** It watches how you play: standing still brings heavier bugs, running around brings fast
  swarms, throwing many bombs brings scattered swarms, low health brings runners; a short message tells you.
- **The platform heals a little** when nothing has bitten for a while.
- Scores of the two games are kept in separate Hall of Fame lists (left/right switches in the Hall of Fame).

## Settings (menu: game settings)

Controls (twin stick / classic tank), frame limit (20, 30 or 60 fps; the game logic keeps its own 60 Hz timer, so a
lower limit only makes the picture less smooth), credits on exit (on / off) and music volume. Left/right or the
select button change a row. They are saved in `.formido/formido.cfg` as `controls`, `frame_limit` and
`credits_on_exit`. Select + Start always leaves at once, without the credits. The settings panel uses
`data/menu2_sf.dat`, made from the upstream `menu2.dat` by `tools/make_menu_clean.py` (the painted key labels are
removed and the rows drawn with the game's own font).

## Build

    ./build_sf3500.sh            # formido.stripped.mipsel (release)
    DEV=1 ./build_sf3500.sh      # formido-dev.stripped.mipsel with FORMIDO_PAD / FORMIDO_SHOT test hooks

Uses the Codescape toolchain and sysroot in `~/handhelds/sf3000-work/buildroot/output/host` (override with
`MIPS_PREFIX` and `SYSROOT`). `-O2` only: no `-ffast-math` (upstream's Makefile has it) and no `-mdspr2`.

## Headless test (QEMU)

    HOME=$PWD FORMIDO_PAD="3000:0x10,3400:0,7000:0x14" FORMIDO_SHOT=8000,11000 \
      qemu-mipsel-static -L <sysroot> ./formido-dev.stripped.mipsel

`FORMIDO_PAD` is `ms:mask,...` (mask bits: Left 1, Right 2, Up 4, Down 8, A 0x10, B 0x20, Start 0x400,
Select 0x800); `FORMIDO_SHOT` saves `shot_<ms>.bmp`. QEMU has no display driver or audio, so those are
only checkable on the device. `FORMIDO_FRAME_MS` sets the minimum time between presented frames
(default 33 ms, about 30 fps; the game logic runs on its own 60 Hz timer).

## Package

`package/` is the card layout: `Formido.sh` and `formido/` (run.sh, binary, data, formido.cfg, boxart.png,
NOTICE.md, licence). Box art: `package-src/boxart.png` is made from the 1254 master
(`../boxart-masters/formido-master-1254.png`) with `../boxart-clean.py`.
