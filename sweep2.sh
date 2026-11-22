#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Combined settings: the same 60 seeds as sweep.sh. Results: botruns/sweep/combo_<name>.txt
cd "$(dirname "$0")"
while IFS='|' read -r label tune; do
	[ -z "$label" ] && continue
	rm -f "botruns/sweep/$label.txt" "botruns/sweep/$label-min.txt"
	TUNE="$tune" ./run_bots.sh native "sweep/$label" mix 7000 60 8
	echo "done $label $(wc -l < botruns/sweep/$label.txt) games"
done <<'LIST'
combo_A_health.5_spawn3|bug_health_mult=0.5,spawn_delay_mult=3
combo_B_health.5_fire3|bug_health_mult=0.5,fire_delay_ticks=3
combo_C_health.5_fire3_spawn3|bug_health_mult=0.5,fire_delay_ticks=3,spawn_delay_mult=3
combo_D_health.5_fire3_spawn4|bug_health_mult=0.5,fire_delay_ticks=3,spawn_delay_mult=4
combo_E_health.5_fire3_spawn4_speed.85|bug_health_mult=0.5,fire_delay_ticks=3,spawn_delay_mult=4,bug_speed_mult=0.85
combo_F_health.5_fire3_spawn4_speed.85_dmg.75|bug_health_mult=0.5,fire_delay_ticks=3,spawn_delay_mult=4,bug_speed_mult=0.85,bug_damage_mult=0.75
LIST
echo SWEEP2-COMPLETE
