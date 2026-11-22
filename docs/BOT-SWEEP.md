# Knob sweep of the enhanced game (bot games, 2026-11-21)

One tuning knob changed at a time, 60 bot games per setting (the same seeds every time, the four bot skills mixed evenly), x86-64 test build, bots on the platform screen only. `./sweep.sh` and `./sweep2.sh` produce the data, `python3 tools/sweep_report.py` the table. Differences of about ten seconds are within the noise between games; use the medians and the middle-half range, not the means (long lucky games pull means up).

```
setting                      n median s  mean s   p25-p75   p90   max kills med kills mean >=2min% >=5min% tyrant%
base (defaults)             60       55    58.9   40-68      98   238        46       51.7       5       0       0
------------------------------------------------------------------------------------------------------------------
bug_damage_mult = 0.5       60       56    73.2   45-71     123   327        48       68.8      10       3       0
bug_damage_mult = 0.75      60       54    65.7   42-69     115   297        44       58.8       8       0       0
bug_damage_mult = 1.5       60       50    51.4   36-62      75   144        36       42.3       3       0       0
------------------------------------------------------------------------------------------------------------------
bug_health_mult = 0.5       60       90   131.1   70-148    313   561       141      184.8      30      12       3
bug_health_mult = 0.75      60       60    76.5   50-77     129   391        58       78.4      10       3       0
bug_health_mult = 1.5       60       40    41.0   32-49      58    60        22       23.4       0       0       0
bug_health_mult = 2         60       30    32.9   26-41      48    54        10       12.4       0       0       0
------------------------------------------------------------------------------------------------------------------
bug_speed_mult = 0.7        60       68    91.7   53-92     205   467        60       80.8      17       3       0
bug_speed_mult = 0.85       60       58    75.1   49-71     147   388        44       65.6      12       2       0
bug_speed_mult = 1.2        60       52    53.4   38-59      67   387        39       44.8       2       2       0
------------------------------------------------------------------------------------------------------------------
contact_speed = 0.3         60       56    67.4   38-71     113   340        45       59.2       8       2       0
contact_speed = 0.45        60       55    72.9   37-67     113   517        43       61.4       8       2       2
contact_speed = 0.8         60       58    58.7   40-64      75   362        46       51.0       3       2       0
contact_speed = 1           60       58    69.2   38-69     120   363        52       63.1      10       2       0
------------------------------------------------------------------------------------------------------------------
fire_delay_ticks = 3        60       82   122.3   66-128    317   492       106      162.0      33      10       0
fire_delay_ticks = 4        60       64    84.8   49-81     132   368        63       85.2      13       5       0
fire_delay_ticks = 7        60       41    41.5   34-50      58    68        23       24.9       0       0       0
------------------------------------------------------------------------------------------------------------------
max_roaming_bugs = 40       60       55    65.2   40-68     133   238        46       61.2      12       0       0
max_roaming_bugs = 60       60       55    60.8   40-68      79   375        46       51.9       5       2       0
max_roaming_bugs = 130      60       55    67.3   40-68      95   501        46       57.6       5       3       2
------------------------------------------------------------------------------------------------------------------
spawn_delay_mult = 0.8      60       44    54.5   34-55      79   292        38       53.4       8       0       0
spawn_delay_mult = 1.6      60       68    76.1   55-83     111   307        59       64.9       7       2       0
spawn_delay_mult = 2        60       72    76.0   55-87      95   317        46       54.5       3       2       0
spawn_delay_mult = 3        60       88    95.2   73-107    143   259        51       62.4      15       0       0
spawn_delay_mult = 4        60      110   112.2   86-129    155   331        64       70.7      40       3       0
------------------------------------------------------------------------------------------------------------------
weight_runner = 0           60       61    63.7   38-84     105   143        48       55.7       2       0       0
weight_runner = 2           60       53    56.9   39-63      70   222        46       52.0       5       0       0
==================================================================================================================
combo_A_health.5_spawn3     60      165   203.5  134-223    410   732       174      226.7      78      17       7
combo_B_health.5_fire3      60      156   327.5   97-302    519  7200       320      378.1      58      25      10
combo_C_health.5_fire3_spa  60      172   316.2  109-280    518  6302       212      263.0      70      23      12
combo_D_health.5_fire3_spa  60      280   300.9  162-402    529   956       296      305.7      83      45      17
combo_E_health.5_fire3_spa  60      291   444.4  193-453    762  7200       278      321.1      90      48      18
combo_F_health.5_fire3_spa  60      334   501.0  185-483    576  7200       348      343.4      87      57      20
```

Reading: bug health, time between shots and the spawn schedule are the strong levers; bug speed and bite damage are modest; contact_speed, max_roaming_bugs and the runner weight barely move a platform-bound bot. Stacked changes multiply: half bug health with a 3-tick fire delay and a 4x slower spawn schedule reaches a median of about 4.7 minutes and meets the first Hive Tyrant in about 1 game in 5, but a few games at those settings never end (cut off at 2 hours), i.e. the loss stops being inevitable. The published chart page is the Artifact 'Formido Knob Sweep'.

## First Tyrant earlier (sweep3.sh, 60 games per row, same seeds)

| Settings | First Tyrant | Median survived | Met a Tyrant | Slain | Died within a minute of it arriving |
|---|---|---|---|---|---|
| default | 8 min | 55 s | 0 % | 0 | n/a |
| default | 5 min | 55 s | 0 % | 0 | n/a |
| default | 1 min | 55 s | 22 % | 0 | 85 % |
| D (half health, 3-tick fire, 4x spawn delay) | 8 min | 280 s | 17 % | 2 | 80 % |
| D | 5 min | 280 s | 40 % | 3 of 24 | 79 % |
| F (D + speed 0.85 + damage 0.75) | 8 min | 334 s | 20 % | 3 | 42 % |
| F | 5 min | 334 s | 53 % | 13 of 32 | 50 % |
| F | 3 min | 238 s | 70 % | 22 of 42 | 36 % |

At the default settings nobody lives to 5 minutes, so moving the Tyrant from 8 to 5 minutes changes nothing; it only matters at
settings where bots survive that long, where it doubles to triples the share that meet one without changing the median.

## Platform healing (sweep4.sh, 60 games per row, Tyrant at 5 minutes)

Default settings: median 52 to 56 s for every healing rate and wait tried (5, 10, 20, 60 hit points a second; waits of 1.5 s, 0.5 s, none):
the bots die to swarm bursts, not to attrition, so healing between fights barely matters. At setting D (half bug health, 3-tick fire,
4x spawn delay): default healing median 280 s, 10 hp/s 336 s, 20 hp/s 310 s, 60 hp/s 330 s; share meeting the first Tyrant 40 % -> 47-57 %.
Most of the gain is already there at 10 hp/s (`platform_regen_ticks` 6); the wait (`platform_regen_delay_ticks`) made no clear difference.
Differences under about 30 s are within the noise between games.

## How bugs take health, and the roaming-and-retreating bot (2026-11-21)

Measured over 100 bot games: bots lose only about 4 hit points per second alive on average, but it comes in bursts: 4 % of ticks
carry damage, most of them 1 hp, some 2 to 4 (a bug in contact bites on most ticks, and several bugs together take health
off at roughly 25 hp/s). From the moment a bot falls below half health it dies in a median of 4 seconds (90th percentile 30 to 63 s).
A roaming bot that retreats to the platform when hurt did no better than the campers (median 46 to 62 s, 0 to 8 % reach 5 minutes,
whatever the healing rate up to 60 hp/s or the wait), but all the bots spend only about 4 to 7 seconds per game on the stone
platform, so these runs do not test the retreat-and-heal pattern. A human can, and that needs the device (or a better bot).
Platform healing now has no wait: it works whenever health is below full and the player stands on the platform.

## Bugs piling up at the edge of a screen (2026-11-21)

Reported: bugs get stuck at the edge of the screen leading to the middle and pool there. Measured with the bots
(12 runs, bug-ticks spent 150+ pixels from the player while hardly moving):

| steering | stalled share | mean survival |
|---|---|---|
| before (critters off-screen still blocked those on-screen) | 85.8 % | 148 s |
| off-screen bugs no longer collide with on-screen ones | 3.5 % | 54 s |
| the same plus a flow field round cliff steps and rock islands | 1.8 % | 61 s |

The queue at the seam was a deadlock: a bug stepping onto the screen collided with the off-screen bugs right behind
the edge, so none of them entered; the column then arrived all at once. The old survival numbers (BOT-RESULTS.md,
the sweeps) were measured with that jam in place, so real survival is shorter than they say (about a minute at the
default settings, not two and a half); the knob comparisons keep their order but the medians would need re-running.
