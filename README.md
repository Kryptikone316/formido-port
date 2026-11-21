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
| R1 | fire along the current aim (in the enhanced game: deploy the bunker) |
| R2 | enhanced game: lay a trap |
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
- **Special ammunition.** Crates of special rounds turn up now and then, only out in the arms (about one every 30 to
  50 seconds while you are there, and they vanish after 50 seconds). Four kinds, each made against one rank: hollow-point
  (drones), flechette (runners), incendiary (strikers), armour-piercing (brutes): about double damage to that rank and a
  little less to the others. Each load is 160 rounds and spoils after 45 seconds, whichever comes first; then you shoot
  normally again. The hive notices what you fire: the longer you use one kind, the fewer of its rank it sends, with a
  message when it adapts.
- **Traps.** Round trap discs turn up in the arms now and then (about one every 70 to 120 seconds while you are out
  there, and 10 % of guardians drop one); each gives a full load of one kind (3 napalm, 4 spikes, 3 glue). You carry one kind at a time: picking up a new disc
  throws away whatever was left of the old one, and a disc of the kind you already carry just fills it up again. **R2** leaves a trap behind you:
  **napalm** (a field of fire, 36 hit points a second to whatever stands in it, burns out in about 16 seconds),
  **spikes** (booby-trapped spears, 22 a second and they slow what is on them, last 40 seconds) or **glue** (a puddle
  that holds bugs almost still in the middle and slows them hard around the edge, 30 seconds). Napalm leaves what it touches burning for 3 more seconds after it leaves the fire (a light damage over time); spikes
  leave it bleeding for 5 seconds (a faint damage over time, and everything else hurts it 25 % more meanwhile).
  A small badge over a bug, or you, shows the effects while they last: a flame (burning), a red drop (bleeding), a blue
  down arrow (slowed). Each trap has an
  integrity of its own, which the bugs wear down, and it fades with time. **The player is not safe on a trap either**:
  the same burning, spearing and sticking applies to you, so stay off. Pickup icons are different in shape as well as
  colour: classic power-ups and cells are as before, special rounds are cartridges with a coloured tip (red drones,
  pale yellow runners, gold strikers, blue brutes), traps are round discs (flame, spears, drop), the bunker kit is a
  green dome with a white rim, healing plants are green.
- **The Hive Tyrant.** After a long time in the game (8 minutes at first, then 7 to 8 minutes after each Tyrant is
  slain) the ground trembles for 15 seconds and a Hive Tyrant walks in from the next screen over: a huge crimson boss
  (4500 health, 30 % more for each one slain before it) with a boss bar at the top. The weakest bugs (drones, runners)
  run from it in terror and the hive sends fewer of them; strikers and brutes screen for it, standing between it and you
  (and more of them come). It has one mutation (two from the second Tyrant on): **brood-mother** (new runners that are not
  afraid of it), **charger** (a "!" tell, then a rush in a straight line: 30 damage; if it hits rock it is dazed and takes
  50 % more damage), **spitter** (acid lands where you stand: a red ring closes in, then a pool for 7 seconds, 2 hit
  points per 5 ticks), **stomper** (a slow wind-up with a ring, then a shockwave 150 pixels around: 28 damage). It is too
  big to be held by glue and barely feels spikes, but napalm and everything else works. Slay it and you are fully healed
  and get **the Tyrant's spoils** for 90 seconds (hits count 60 % more, you fire faster, move 20 % quicker and mend
  1 hit point every half second). Then things settle down again, except that every bug is now 3 % tougher and 1.5 %
  faster for good, for every Tyrant slain so far, until the next one comes.
- **Legend (L2).** In the enhanced game L2 pauses and shows what every icon means (power cells, special rounds, trap discs,
  bunker kit, healing plant, classic pickups, red plants, the corner map, the three status badges, napalm, spikes,
  glue, the Tyrant, the burrower's tremor) plus a light stats strip: time survived, score, Tyrants slain, hive level,
  cells delivered and kills per rank. Any button resumes.
- **Clean exit and map edges.** Select + Start always leaves at once (no name entry, no credits). The centre screen has
  rock blocks in its four corners where the arms' cliffs meet it, so the boundary is visible and nothing invisible catches
  you; the open way into each arm is the middle of the edge.
- **Reset Hall of Fame.** In game settings, "reset hall of fame": press select once to be asked, again to erase both lists.
- **Hallucinations.** While the mushroom's effect lasts (the screen-warping one), violet, flickering phantoms appear
  around you every 2 to 4 seconds, up to 6 at once. They are slow-witted but bite like real bugs (60 health, 2 damage), and
  they vanish the instant the effect ends. Killing one has a 30 % chance to leave a (half-size) healing plant where it
  fell, so the mushroom is not a pure penalty. A phantom may also leave a teal **spore pod**
  (12 % of the kills that do not leave a plant): a mushroom with a different look, with the same effect when you walk into
  it. To stop it turning into one mushroom after another: only one pod can lie about, it rots after 25 seconds, and once
  you take one no more pods drop for 150 seconds. They are not afraid of the Tyrant and do not count as its screen.
- **Bunker.** A rare kit that only a guardian can drop (15 % per guardian, one in three for the warden; never more than
  one kit or bunker at a time). Walk over the kit to take it, then press **R1** to deploy the bunker a little ahead of
  you and step onto it. Inside you cannot walk, but you can turn, aim and fire out; you mend 3 hit points a second, and
  the bugs wear the bunker down instead of you (it takes about a third of each bite; 800 points of integrity, shown
  as a bar, and it fatigues slowly on its own; an empty bunker crumbles after 90 seconds). When it expires it goes off
  like a small mine and hurts the bugs crowding it, so you are never trapped, and you carry on normally. In the enhanced
  game R1 is the bunker button, so A (without L1) is the way to fire along the aim.
- **Laser sight.** A thin red line from the muzzle along the line of fire (up to 260 pixels, ending at a wall, crag or bug,
  with a small dot). Settings: off, enhanced (the default: only in the enhanced game) or always.
- **No dead-end slots.** The arms' collision is cleaned so that every gap is at least body-wide (about 40 pixels): the
  narrow slots, cracks and gorges along the cliffs that a player and a bug could wedge in are closed up as rock (the
  pictures show them as rock too), and every arm is one connected area open to the centre screen.
- **Walkable ground.** The arms' collision maps are cleaned so that nothing thin, no tiny pocket and no unreachable
  corner is left to catch on, and crags keep clear of the cliffs. Anything (player or bug) that ends up inside rock,
  shoved there by a swarm, a blast or a burst, is put on the nearest open ground at once.
- **No recoil.** Firing does not push you or shake your aim with the twin-stick preset or in the enhanced game; the
  classic game with the original tank controls keeps the original recoil.
- **Red plants are thorns.** They stand on open sand, clear of rock. You can walk straight through a patch: touching one
  costs 1 hit point, and standing in it costs about 1 every 25 ticks (a scratch, nothing more).
- **Healing plants.** A rare green plant grows in the arms (never on the platform screen): standing within about 50
  pixels mends you at 5 hit points a second, up to 40 in all, and it yellows, browns and withers as it is used (or after
  70 seconds), then disappears. The wait before the next one is 2 to 3.5 minutes, so the platform stays the reliable way
  to heal.
- **Bug ranks.** Drone (the classic bug), runner (small, fast, in packs), striker (tough, hits harder), brute (huge,
  slow). They are the classic bug's pictures scaled and recoloured. They appear as the match goes on.
- **Burrowers.** A fifth rank that lives under the sand of the arms (never in the centre screen, never near the
  pylons). It is not drawn and cannot be hit while hidden. It creeps towards you under the surface, leaving only a
  faint trail of dust, and when close it rumbles for under a second ("the ground trembles", dust building) and erupts at
  the spot it reached: 25 damage and a shove if you are right on top of it, so a player who keeps moving is fine and one
  who stands still, or is busy fighting, is not. After that it fights like a bug (earth-and-bone coloured, tougher than
  a drone) and dives again, healed, if you leave the arm. They are rare: the first after about a minute of the match,
  then at most one per arm (two after 8 minutes), a new one every 80 to 140 seconds while you are out in an arm.
- **The hive adapts.** It watches how you play: standing still brings heavier bugs, running around brings fast
  swarms, throwing many bombs brings scattered swarms, low health brings runners; a short message tells you.
- **The platform heals a little** when nothing has bitten for a while.
- Scores of the two games are kept in separate Hall of Fame lists (left/right switches in the Hall of Fame).

## Settings (menu: game settings)

Controls (twin stick / classic tank), laser sight (off / enhanced / always), frame limit (20, 30 or 60 fps; the game logic keeps its own 60 Hz timer, so a
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
