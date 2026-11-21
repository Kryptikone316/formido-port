# Formido for the Datafrog SF3500

[Formido](https://mhgames.org/formido/) 1.0.1 by Mika Halttunen (GPL v2 or later), ported to the
Datafrog SF3500 handheld (MIPS32r2, SDL 1.2, no audio device). Upstream sources are in `src/` and
`include/`; every change is under `#ifdef SF3500`. The device glue is in `sf3500/`.

## Controls

| Button | Menus | In a match |
|---|---|---|
| D-pad | choose | up/down move, left/right turn |
| A | select | shoot |
| B | back | special (throw the power-up you hold) |
| Start | select | pause |
| Select | back | end the match |
| Select + Start | leave the game from anywhere | |

High-score name: Up/Down change the letter, Right takes it, Left or B steps back, A finishes.

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
