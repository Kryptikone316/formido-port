# Bot test runs of the enhanced game (2026-11-21)

500 bot-played games (5 batches of 100; 125 each of four skill levels), x86-64 test build, default tuning
(`contact_speed` 0.6). The bots stay on the platform screen: they never fetch power cells, special ammunition, traps or
the bunker kit from the arms, so this is the game *without the arm economy*, a conservative baseline. Skill 0 reacts in
200 ms with sloppy aim, 1 in 150 ms, 2 in 100 ms, 3 (expert) in 50 ms; all look ahead for the safest of nine moves, aim
along the eight-way line that hits the closest bug, and use health, bombs and mines they can reach.

| | survived (s) | kills |
|---|---|---|
| mean | 61 | 54 |
| median | 56 | 47 |
| 10th percentile | 31 | |
| 90th percentile | 80 | 96 |
| longest | 509 | 312 |

Share of runs still alive: 30 s 91 %, 45 s 70 %, 60 s 39 %, 75 s 13 %, 90 s 8 %, 2 min 4 %, 3 min 2 %.
The five batches agree (batch means 58 to 65 s). Skill barely matters (means 58, 63, 67, 58 s for skills 0 to 3): the limit is
the weapon (one hitscan target, about 0.9 kills a second at best) against the spawn ramp (about 2.5 spawn events a second
from 80 seconds on, runner packs of 3 to 5), not the player.
Kills by rank per game: drones 39, runners 12.5, strikers 1.7, brutes 0.3. A Hive Tyrant appeared in 1 run of 500 and was never
slain; burrowers need the arms, so none were met.

Cross-check against the real MIPS code under QEMU (16 paired games, same seeds, skill 1, timer frozen so both run exactly
3 ticks per pass): median 52 s vs 54 s native, median kills 45.5 vs 48. The shapes agree; native shows a longer lucky tail.
What the native build cannot say: the device's speed (whether 60 ticks a second holds with 90 bugs) and any effect of its
slightly different maths.
