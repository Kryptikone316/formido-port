#!/bin/sh
# One-knob-at-a-time sweep: every setting plays the same 60 seeds (an even mix of the four bot skills) with native builds.
# Results: botruns/sweep/<label>.txt (one RESULT line per game). Run on the host: flatpak-spawn --host sh -c "cd <dir> && ./sweep.sh"
cd "$(dirname "$0")"
mkdir -p botruns/sweep
while IFS='|' read -r label tune; do
	[ -z "$label" ] && continue
	rm -f "botruns/sweep/$label.txt" "botruns/sweep/$label-min.txt"
	TUNE="$tune" ./run_bots.sh native "sweep/$label" mix 7000 60 8
	echo "done $label $(wc -l < botruns/sweep/$label.txt) games"
done <<'LIST'
base|
spawn_delay_mult=0.8|spawn_delay_mult=0.8
spawn_delay_mult=1.6|spawn_delay_mult=1.6
spawn_delay_mult=2.0|spawn_delay_mult=2.0
spawn_delay_mult=3.0|spawn_delay_mult=3.0
spawn_delay_mult=4.0|spawn_delay_mult=4.0
bug_damage_mult=0.5|bug_damage_mult=0.5
bug_damage_mult=0.75|bug_damage_mult=0.75
bug_damage_mult=1.5|bug_damage_mult=1.5
bug_health_mult=0.5|bug_health_mult=0.5
bug_health_mult=0.75|bug_health_mult=0.75
bug_health_mult=1.5|bug_health_mult=1.5
bug_health_mult=2.0|bug_health_mult=2.0
bug_speed_mult=0.7|bug_speed_mult=0.7
bug_speed_mult=0.85|bug_speed_mult=0.85
bug_speed_mult=1.2|bug_speed_mult=1.2
contact_speed=0.3|contact_speed=0.3
contact_speed=0.45|contact_speed=0.45
contact_speed=0.8|contact_speed=0.8
contact_speed=1.0|contact_speed=1.0
fire_delay_ticks=3|fire_delay_ticks=3
fire_delay_ticks=4|fire_delay_ticks=4
fire_delay_ticks=7|fire_delay_ticks=7
max_roaming_bugs=40|max_roaming_bugs=40
max_roaming_bugs=60|max_roaming_bugs=60
max_roaming_bugs=130|max_roaming_bugs=130
weight_runner=0|weight_runner=0
weight_runner=2|weight_runner=2
LIST
echo SWEEP-COMPLETE
