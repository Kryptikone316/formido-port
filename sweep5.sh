#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Mobile bot variants (skills 4 and 5 mixed): 40 games each, tyrant at 5 min, regen 10 hp/s defaults unless the label says otherwise.
cd "$(dirname "$0")"
while IFS='|' read -r label env tune; do
	[ -z "$label" ] && continue
	rm -f "botruns/sweep/$label.txt"
	env $env TUNE="$tune" ./run_bots.sh native "sweep/$label" mixr 7000 40 8
	echo "done $label $(wc -l < botruns/sweep/$label.txt) games"
done <<'LIST'
mob_a235_lo100_pull9000|FORMIDO_BOTARENA=235 FORMIDO_BOTLO=100 FORMIDO_BOTPULL=9000|
mob_a200_lo120_pull3000|FORMIDO_BOTARENA=200 FORMIDO_BOTLO=120 FORMIDO_BOTPULL=3000|
mob_a170_lo120_pull3000|FORMIDO_BOTARENA=170 FORMIDO_BOTLO=120 FORMIDO_BOTPULL=3000|
mob_a170_lo140_pull1500|FORMIDO_BOTARENA=170 FORMIDO_BOTLO=140 FORMIDO_BOTPULL=1500|
mob_a140_lo120_pull3000|FORMIDO_BOTARENA=140 FORMIDO_BOTLO=120 FORMIDO_BOTPULL=3000|
LIST
echo SWEEP5-COMPLETE
