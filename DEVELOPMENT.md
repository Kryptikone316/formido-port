# Formido on the SF3500: development notes

For people who build, test or tune the port. Players need only the README.

## Where things are

Upstream sources are in `src/` and `include/`; every change to them is under `#ifdef SF3500`. The device glue (display, gamepad, audio mixing, image loading) is in `sf3500/`. The enhanced game is `src/enhanced.cpp` with its numbers in `include/tuning.h` / `src/tuning.cpp`. `tools/` holds the scripts that generate the enhanced game's art and sounds and the test tools. `NOTICE.md` has the licence and provenance table.

Note: the sprite's gun hangs 60 degrees left of the body and bullets leave along `angle - 60`; twin-stick mode keeps a
separate `aim` and sets `angle = aim + 60`, so the picture and the bullets agree.

## Build

    ./build_sf3500.sh            # formido.stripped.mipsel (release)
    DEV=1 ./build_sf3500.sh      # formido-dev.stripped.mipsel with FORMIDO_PAD / FORMIDO_SHOT test hooks

Uses the Codescape toolchain and sysroot in `~/handhelds/sf3000-work/buildroot/output/host` (override with
`MIPS_PREFIX` and `SYSROOT`). `-O2` only: no `-ffast-math` (upstream's Makefile has it) and no `-mdspr2`.

## Bot test runs

A bot plays the enhanced game through the same button path as a person, which is how the settings were tuned: see
`docs/TUNING-NOTES.md` for what it showed and what came of it. `build_native.sh` makes an x86-64 test build with the test hooks
and `run_bots.sh native <batch> <skill> <first seed> <count>` plays bot games with it at full speed; `tools/bot_stats.py`
summarises the results. The x86-64 build is for statistics only: the device's CPU and maths differ.

## Collision stress test

`python3 tools/collision_sim.py data [--runs N] [--fast] [--substep]` reproduces the game's movement rules on the real
collision maps of the five screens and shoves a simulated player into walls, corners and cracks, counting tunnelling,
being left inside rock, teleports by the unstick step, hangs (no direction frees the player) and pockets. It also times
how long it takes to get away from a wall freely and with a bug in contact. Run it after regenerating the arm maps.

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

## The tuning file in detail

All the numbers that decide how hard or generous the enhanced game is (56 of them: player speed and healing, bug health,
speed, damage and spawn rate, how soon the Hive Tyrant comes and how strong it is, the burrowers and their stun, power-cell
guards and respawn times, ammunition, the bunker, traps, healing plants, hallucinations and spores) are in `formido_tuning.cfg`
in the port folder, each with a comment saying what it does and its allowed range. It is read at the start of every
enhanced match, so edit it with a text editor and start a new game to try a change. Values outside their range are brought
back into it, unknown or unreadable lines are reported in `log.txt`, and if the file is missing it is written again with
the defaults; knobs added by a newer version are appended without touching your edits. The classic game ignores it.
`contact_speed` (default 0.6) is the share of your movement you keep while a bug overlaps you; the classic game's 0.3 is
what made swarms hard to get away from.


## Further notes

- [`docs/ENHANCED-GAME-DETAILS.md`](docs/ENHANCED-GAME-DETAILS.md): every enhanced-game mechanic with its numbers.
- [`docs/TUNING-NOTES.md`](docs/TUNING-NOTES.md): a summary of what the bot testing showed and what it led to.
- [`docs/BOT-RESULTS.md`](docs/BOT-RESULTS.md) and [`docs/BOT-SWEEP.md`](docs/BOT-SWEEP.md): the raw numbers of the bot runs (from before the pile-up fix, so survival times are optimistic).
