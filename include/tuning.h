/*
    Formido - Datafrog SF3500 port: tuning knobs of the enhanced game.

    Every number that decides how hard or how generous the enhanced game is lives in the table below (name, default,
    lowest, highest, section, what it does). The game reads `formido_tuning.cfg` from the port folder at the start of each
    enhanced match, so the numbers can be changed with a text editor and tried at once. The file is written with all
    the defaults and descriptions the first time it is missing; knobs added by a newer version are appended to an
    existing file without touching what is there. Values outside the range are clamped. The classic game ignores all of it.
*/

#ifndef TUNING_H
#define TUNING_H

#ifdef SF3500

//        name                    default   lo     hi     section      description
#define TUNING_LIST(X) \
	X(contact_speed,             0.60f,  0.10f,   1.00f, "player",    "share of your movement kept while a bug overlaps you (the classic game keeps 0.30)") \
	X(carry_speed,               0.88f,  0.30f,   1.50f, "player",    "walking speed while carrying a power cell (1.0 = full speed)") \
	X(fire_delay_ticks,          5.0f,   2.0f,   15.0f,  "player",    "ticks between shots (60 ticks = 1 second; lower fires faster)") \
	X(platform_regen_ticks,      6.0f,   1.0f,  120.0f,  "player",    "natural healing on the stone platform: ticks per hit point (60 ticks = 1 second; 6 = 10 hit points a second; lower = faster)") \
	X(platform_regen_delay_ticks, 90.0f, 0.0f,  600.0f,  "player",    "the platform starts healing you after this many ticks without damage (90 = a second and a half)") \
	X(medbay_regen_ticks,        4.0f,   1.0f,  120.0f,  "player",    "the same as platform_regen_ticks with a south (medbay) cell active (4 = 15 hit points a second)") \
	X(medbay_regen_delay_ticks,  30.0f,  0.0f,  600.0f,  "player",    "the same as platform_regen_delay_ticks with a south (medbay) cell active") \
	X(bug_health_mult,           1.00f,  0.30f,   5.00f, "bugs",      "all bugs' health (except the Tyrant) times this") \
	X(bug_speed_mult,            1.00f,  0.50f,   2.00f, "bugs",      "all bugs' walking speed times this") \
	X(bug_damage_mult,           1.00f,  0.25f,   5.00f, "bugs",      "damage of every bite on the player times this") \
	X(spawn_delay_mult,          1.20f,  0.40f,   5.00f, "bugs",      "time between hive spawns times this (the classic game's schedule is 1.0; higher = fewer bugs)") \
	X(max_roaming_bugs,          90.0f,  20.0f,  160.0f, "bugs",      "most bugs roaming at once (guards waiting at their posts do not count)") \
	X(weight_runner,             1.00f,  0.00f,  10.00f, "bugs",      "how often runners are picked (0 = never, 2 = twice as often)") \
	X(weight_striker,            1.00f,  0.00f,  10.00f, "bugs",      "how often strikers are picked") \
	X(weight_brute,              1.00f,  0.00f,  10.00f, "bugs",      "how often brutes are picked") \
	X(time_toughening,           0.40f,  0.00f,   3.00f, "bugs",      "extra health of the stronger ranks after 15 minutes (0.4 = +40 %)") \
	X(hive_health_per_tyrant,    0.03f,  0.00f,   0.50f, "tyrant",    "every bug's health grows by this share for each Tyrant slain (0.03 = +3 %)") \
	X(hive_speed_per_tyrant,     0.015f, 0.00f,   0.20f, "tyrant",    "every bug's speed grows by this share for each Tyrant slain") \
	X(tyrant_first_minutes,      5.0f,   0.5f,   60.0f,  "tyrant",    "minutes until the first Hive Tyrant stirs") \
	X(tyrant_interval_minutes,   7.0f,   0.5f,   60.0f,  "tyrant",    "minutes between a Tyrant's death and the next one stirring") \
	X(tyrant_health,             4500.0f, 300.0f, 30000.0f, "tyrant", "the first Tyrant's health") \
	X(tyrant_health_growth,      0.30f,  0.00f,   2.00f, "tyrant",    "each Tyrant after the first has this much more health than the one before (0.3 = +30 %)") \
	X(boon_seconds,              90.0f,  5.0f,  600.0f,  "tyrant",    "how long the Tyrant's spoils last") \
	X(boon_damage_mult,          1.60f,  1.00f,   5.00f, "tyrant",    "your damage during the spoils") \
	X(boon_speed_mult,           1.20f,  1.00f,   2.00f, "tyrant",    "your walking speed during the spoils") \
	X(boon_fire_delay_ticks,     3.0f,   1.0f,   15.0f,  "tyrant",    "ticks between shots during the spoils") \
	X(burrower_damage,           25.0f,  0.0f,   100.0f, "burrowers", "damage of a burrower's eruption") \
	X(burrower_first_seconds,    60.0f,  0.0f,  900.0f,  "burrowers", "no burrower in the first this many seconds") \
	X(burrower_min_seconds,      80.0f,  10.0f, 900.0f,  "burrowers", "a new burrower may come after at least this many seconds in an arm ...") \
	X(burrower_max_seconds,      140.0f, 10.0f, 900.0f,  "burrowers", "... and at most this many") \
	X(stunner_chance_first,      0.30f,  0.00f,   1.00f, "burrowers", "share of burrowers that stun, after the first Tyrant has been slain") \
	X(stunner_chance_step,       0.15f,  0.00f,   1.00f, "burrowers", "added for each further Tyrant slain (the total is capped at 0.8)") \
	X(stunner_damage,            10.0f,  0.0f,   100.0f, "burrowers", "damage of a stunning burrower's eruption") \
	X(stun_ticks,                75.0f,  10.0f, 300.0f,  "burrowers", "ticks you cannot walk when stunned") \
	X(stun_immune_seconds,       5.0f,   0.0f,   60.0f,  "burrowers", "seconds after a stun in which you cannot be stunned again") \
	X(garrison_min,              0.75f,  0.30f,   3.00f, "cells",     "weakest guard of a power cell (1.0 = the standard garrison)") \
	X(garrison_max,              1.35f,  0.30f,   3.00f, "cells",     "strongest guard of a power cell") \
	X(cell_respawn_min_seconds,  100.0f, 10.0f, 900.0f,  "cells",     "a delivered cell returns after at least this many seconds ...") \
	X(cell_respawn_max_seconds,  160.0f, 10.0f, 900.0f,  "cells",     "... and at most this many") \
	X(pylon_mode_seconds,        120.0f, 10.0f, 900.0f,  "cells",     "how long a delivered cell sets the pylons' mode") \
	X(ammo_rounds,               160.0f, 10.0f, 999.0f,  "items",     "rounds in a load of special ammunition") \
	X(ammo_seconds,              45.0f,  5.0f,  600.0f,  "items",     "special rounds spoil after this many seconds") \
	X(ammo_bonus_scale,          1.00f,  0.00f,   3.00f, "items",     "scales the bonus damage of special rounds (0 = none, 2 = twice the bonus)") \
	X(ammo_min_seconds,          32.0f,  5.0f,  900.0f,  "items",     "a crate of special rounds turns up after at least this many seconds in an arm ...") \
	X(ammo_max_seconds,          50.0f,  5.0f,  900.0f,  "items",     "... and at most this many") \
	X(bunker_integrity,          800.0f, 100.0f, 5000.0f, "items",    "how much the bunker can take") \
	X(bunker_absorb,             0.35f,  0.05f,   1.00f, "items",     "share of a bite that reaches the bunker instead of you") \
	X(bunker_blast_damage,       190.0f, 0.0f,  1000.0f, "items",     "damage of the blast when the bunker expires (falls off with distance)") \
	X(bunker_drop_chance,        0.15f,  0.00f,   1.00f, "items",     "chance that a guardian drops a bunker kit") \
	X(warden_drop_chance,        0.333f, 0.00f,   1.00f, "items",     "the same for a warden (a brute guardian)") \
	X(trap_damage_mult,          1.00f,  0.10f,   5.00f, "items",     "damage of napalm and spikes (to bugs and to you) times this") \
	X(plant_pool,                40.0f,  5.0f,  200.0f,  "items",     "hit points a healing plant gives in all") \
	X(plant_min_seconds,         120.0f, 10.0f, 900.0f,  "items",     "a new healing plant after at least this many seconds ...") \
	X(plant_max_seconds,         200.0f, 10.0f, 900.0f,  "items",     "... and at most this many") \
	X(phantom_max,               6.0f,   0.0f,   12.0f,  "mushroom",  "most hallucinations at once") \
	X(spore_chance,              0.12f,  0.00f,   1.00f, "mushroom",  "chance a phantom's death leaves a spore pod") \
	X(spore_lockout_seconds,     150.0f, 0.0f,  900.0f,  "mushroom",  "no spore pods for this many seconds after you take one") \
	X(bleed_vulnerability,       1.25f,  1.00f,   3.00f, "traps",     "damage multiplier on whatever is bleeding (spikes)")

struct TUNING {
#define X(name, def, lo, hi, sec, desc) float name;
	TUNING_LIST(X)
#undef X
};

extern TUNING tune;

// Fill in the defaults, then read the file (write it first if it does not exist). Call at the start of every match.
void tune_load(const char *path);

#endif
#endif
