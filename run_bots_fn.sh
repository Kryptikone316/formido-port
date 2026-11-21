HERE=${HERE:-$PWD}
SR=$HOME/handhelds/sf3000-work/buildroot/output/host/mipsel-buildroot-linux-gnu/sysroot
one() {
	MODE=$1; BATCH=$2; SKILL=$3; SEED=$4
	if [ "$SKILL" = mix ]; then SKILL=$((SEED % 4)); fi		# skill follows the seed: an even mix of the four levels
	D=botruns/$BATCH/s$SKILL-$SEED
	rm -rf "$D"; mkdir -p "$D/.formido"; ln -s "$HERE/data" "$D/data"; cp formido.cfg "$D/"
	# TUNE="name=value,name=value": tuning-file overrides for this run (the other knobs keep their defaults)
	if [ -n "${TUNE:-}" ]; then echo "$TUNE" | tr ',' '\n' | sed 's/=/ = /' > "$D/formido_tuning.cfg"; fi
	cd "$D"
	if [ "$MODE" = native ]; then
		HOME=$PWD SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy FORMIDO_BOT=$SKILL FORMIDO_SEED=$SEED FORMIDO_FAST=1 timeout 1200 "$HERE/formido-native" > out.txt 2>&1 || true
	else
		HOME=$PWD FORMIDO_BOT=$SKILL FORMIDO_SEED=$SEED FORMIDO_FAST=1 timeout 3000 qemu-mipsel-static -L "$SR" "$HERE/formido-dev.stripped.mipsel" > out.txt 2>&1 || true
	fi
	grep -h "^RESULT" out.txt >> "$HERE/botruns/$BATCH.txt" || echo "NORESULT skill=$SKILL seed=$SEED" >> "$HERE/botruns/$BATCH.txt"
	grep -h "^MIN" out.txt | sed "s/^/skill=$SKILL seed=$SEED /" >> "$HERE/botruns/$BATCH-min.txt" || true
	cd "$HERE"; rm -rf "$D"
}
