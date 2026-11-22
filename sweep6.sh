#!/bin/sh
# Mobile bot variants again, with the new default of no healing wait (platform_regen_delay_ticks 0).
cd "$(dirname "$0")"
while IFS='|' read -r label env tune; do
	[ -z "$label" ] && continue
	rm -f "botruns/sweep/$label.txt"
	env $env TUNE="$tune" ./run_bots.sh native "sweep/$label" mixr 7000 40 8
	echo "done $label $(wc -l < botruns/sweep/$label.txt) games"
done <<'LIST'
mobd0_a235_lo100_pull9000|FORMIDO_BOTARENA=235 FORMIDO_BOTLO=100 FORMIDO_BOTPULL=9000|
mobd0_a200_lo120_pull3000|FORMIDO_BOTARENA=200 FORMIDO_BOTLO=120 FORMIDO_BOTPULL=3000|
mobd0_a170_lo120_pull3000|FORMIDO_BOTARENA=170 FORMIDO_BOTLO=120 FORMIDO_BOTPULL=3000|
mobd0_a170_lo140_pull1500|FORMIDO_BOTARENA=170 FORMIDO_BOTLO=140 FORMIDO_BOTPULL=1500|
mobd0_a140_lo120_pull3000|FORMIDO_BOTARENA=140 FORMIDO_BOTLO=120 FORMIDO_BOTPULL=3000|
LIST
echo SWEEP5-COMPLETE
