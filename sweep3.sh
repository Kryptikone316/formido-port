#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# First Hive Tyrant earlier: the same 60 seeds as sweep.sh. Results: botruns/sweep/ty_<name>.txt
cd "$(dirname "$0")"
while IFS='|' read -r label tune; do
	[ -z "$label" ] && continue
	rm -f "botruns/sweep/$label.txt" "botruns/sweep/$label-min.txt"
	TUNE="$tune" ./run_bots.sh native "sweep/$label" mix 7000 60 8
	echo "done $label $(wc -l < botruns/sweep/$label.txt) games"
done <<'LIST'
ty_base_5min|tyrant_first_minutes=5
ty_base_1min|tyrant_first_minutes=1
ty_D_5min|bug_health_mult=0.5,fire_delay_ticks=3,spawn_delay_mult=4,tyrant_first_minutes=5
ty_F_5min|bug_health_mult=0.5,fire_delay_ticks=3,spawn_delay_mult=4,bug_speed_mult=0.85,bug_damage_mult=0.75,tyrant_first_minutes=5
ty_F_3min|bug_health_mult=0.5,fire_delay_ticks=3,spawn_delay_mult=4,bug_speed_mult=0.85,bug_damage_mult=0.75,tyrant_first_minutes=3
LIST
echo SWEEP3-COMPLETE
