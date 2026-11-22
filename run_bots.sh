#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# usage: run_bots.sh <native|mips> <batch> <skill> <first seed> <count> [parallel]
# Plays bot games with the test build; one RESULT line per game goes to botruns/<batch>.txt (the per-minute lines to botruns/<batch>-min.txt).
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
MODE=$1; BATCH=$2; SKILL=$3; FIRST=$4; COUNT=$5; PAR=${6:-8}
SR=$HOME/handhelds/sf3000-work/buildroot/output/host/mipsel-buildroot-linux-gnu/sysroot
cd "$HERE"
mkdir -p botruns/$BATCH
i=0
while [ $i -lt $COUNT ]; do
	echo $((FIRST + i))
	i=$((i + 1))
done | xargs -P "$PAR" -I{} sh -c "HERE=$HERE; . ./run_bots_fn.sh; one $MODE $BATCH $SKILL {}"
