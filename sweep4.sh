#!/bin/sh
# Platform healing: the same 60 seeds as sweep.sh. Results: botruns/sweep/heal_<name>.txt
cd "$(dirname "$0")"
D="bug_health_mult=0.5,fire_delay_ticks=3,spawn_delay_mult=4"
while IFS='|' read -r label tune; do
	[ -z "$label" ] && continue
	rm -f "botruns/sweep/$label.txt" "botruns/sweep/$label-min.txt"
	TUNE="$tune" ./run_bots.sh native "sweep/$label" mix 7000 60 8
	echo "done $label $(wc -l < botruns/sweep/$label.txt) games"
done <<LIST
heal_base_5min|tyrant_first_minutes=5
heal_base_regen6|platform_regen_ticks=6
heal_base_regen3|platform_regen_ticks=3
heal_base_regen1|platform_regen_ticks=1
heal_base_delay30|platform_regen_delay_ticks=30
heal_base_delay0|platform_regen_delay_ticks=0
heal_base_regen3_delay0|platform_regen_ticks=3,platform_regen_delay_ticks=0
heal_D_regen12|$D
heal_D_regen6|$D,platform_regen_ticks=6
heal_D_regen3|$D,platform_regen_ticks=3
heal_D_regen1|$D,platform_regen_ticks=1
heal_D_delay30|$D,platform_regen_delay_ticks=30
heal_D_regen3_delay0|$D,platform_regen_ticks=3,platform_regen_delay_ticks=0
LIST
echo SWEEP4-COMPLETE
