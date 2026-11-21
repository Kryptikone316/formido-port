# Formido for the Datafrog SF3500

[Formido](https://mhgames.org/formido/) 1.0.1 by Mika Halttunen (GPL v2 or later), ported to the
Datafrog SF3500 handheld (MIPS32r2, SDL 1.2, no audio device). Upstream sources are in `src/` and
`include/`; every change is under `#ifdef SF3500`. The device glue is in `sf3500/`.

## Controls

Menus: D-pad chooses, A or Start selects, B or Select goes back.

In a match (default, `controls = 1` in `formido.cfg`): twin-stick style.

| Button | Action |
|---|---|
| D-pad | walk in the direction pressed (8 ways); you face the way you walk |
| X / A / B / Y | aim and fire up / right / down / left; two together aim diagonally; you can walk one way and fire another |
| R1 | fire along the current aim |
| L1 | special (throw the power-up you hold, in the aim direction) |
| Start | pause |
| Select | end the match |
| Select + Start | leave the game from anywhere |

`controls = 0` restores the original tank controls: Left/Right turn, Up/Down walk along the facing, A shoots,
B is special.

High-score name: Up/Down change the letter, Right takes it, Left or B steps back, A finishes.

Note: the sprite's gun hangs 60 degrees left of the body and bullets leave along `angle - 60`; twin-stick mode keeps a
separate `aim` and sets `angle = aim + 60`, so the picture and the bullets agree.

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
