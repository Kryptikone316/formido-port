# How the enhanced game's settings were arrived at

The enhanced game is meant to be unwinnable: the hive keeps sending more, and the question is how long you last and what
you can do with the time. To tune it without a person playing hundreds of games, a bot played the enhanced game many times
(a fast PC build, with a sample of games cross-checked against the real MIPS code under emulation), first with the default
numbers and then changing one setting at a time. This is a summary of what that showed and what came of it. The raw runs
are not kept here; the test hooks and scripts that produced them are (`run_bots.sh`, `sweep*.sh`, `tools/bot_stats.py`).

## What the bots showed

- **The player's skill hardly mattered.** Fast and slow bots, sloppy and careful aim, ended up about the same. What decides
  a game is the gun (one target at a time) against how fast the hive ramps up, not how well you play.
- **Four settings move survival the most:** how fast the gun fires, how much health the bugs have, how often the hive spawns,
  and how fast the bugs walk. Others had little or no effect on their own: how many bugs may roam at once, how often
  runners are picked, and how strongly a bug in contact slows you.
- **The unwinnable design can be eased.** Several gentler settings together make a game last several times longer, and the
  Hive Tyrant then turns up in a good share of games instead of almost none. So the knobs really decide how unfair it is.
- **Healing:** the platform's healing rate made little difference to the bots beyond about 10 hit points a second, and the bots
  could not do what a person does (roam, retreat to the platform below some health, heal while still fighting). Bites arrive
  in bursts when several bugs are in contact, so a player at half health can be dead within seconds. Whether the retreat-and-heal
  rhythm works was left for play on the device, where it was found to work.
- **Bug pile-ups at the edge of a screen.** The runs showed most of the bugs far from the player standing still: bugs
  waiting just off-screen blocked the ones entering, and arrived in a clump. That was fixed (off-screen bugs no longer block
  on-screen ones, and bugs steer round rock), which also made the game harder than the first runs suggested.
- **A bug in the original:** none was found in the game itself by the bots, but the collision stress test found hang spots in
  the arm maps, which were fixed (see the README).

## What went into the settings file

`formido_tuning.cfg` holds every number (about 70), each with its default, its range and a line saying what it does. The
defaults the testing settled on, in plain terms:

- the platform heals 10 hit points a second whenever you are hurt and standing on it, with no waiting (15 with a south cell);
- the first Hive Tyrant stirs after 5 minutes, and the next 7 minutes after one is slain, each stronger than the last;
- bug bites are 20 % lighter and bugs 5 % slower than the first tuning, so there is room to escape and heal;
- the hive sends runners in smaller packs (2 to 3, at most 10 alive), so a wave can be shot down;
- a bug in contact slows you to 60 % instead of the classic 30 %, so swarms can be broken out of;
- the flamethrower drops after 60 kills, the rank bonus is 4 % per rank (15 kills each, up to 5 ranks), and special ammunition,
  traps, the bunker kit and healing plants are all finite or rare.

These can all be changed by editing the file and starting a new game; the classic game ignores it.
