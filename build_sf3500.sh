#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Builds formido.mipsel for the Data Frog SF3500 (MIPS32r2 74Kc, hard float, SDL 1.2 on the device).
#
#   MIPS_PREFIX=/path/to/mips-mti-linux-gnu- SYSROOT=/path/to/sysroot ./build_sf3500.sh
#
# Defaults point at the sf3000-work Buildroot host tools. -O2 only: no -ffast-math and no -mdspr2
# (upstream's Makefile uses -ffast-math; a device-only fault was traced to it in the Quake ports).
set -eu
HOST=${HOST_DIR:-$HOME/handhelds/sf3000-work/buildroot/output/host}
MIPS_PREFIX=${MIPS_PREFIX:-$HOST/bin/mips-mti-linux-gnu-}
SYSROOT=${SYSROOT:-$HOST/mipsel-buildroot-linux-gnu/sysroot}

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CXX=${CXX:-${MIPS_PREFIX}g++}
STRIP=${STRIP:-${MIPS_PREFIX}strip}
cd "$HERE"   # compile with relative paths, so the binary does not depend on where the tree is

DEVFLAG=""; OUTNAME=formido; OBJ=obj-sf3500
if [ "${DEV:-0}" = 1 ]; then DEVFLAG="-DFORMIDO_TESTHOOKS"; OUTNAME=formido-dev; OBJ=obj-sf3500-dev; fi   # DEV=1: test build with the FORMIDO_PAD / FORMIDO_SHOT hooks
FLAGS="-EL -mips32r2 -mtune=74kc -mhard-float --sysroot=$SYSROOT -O2 -DSF3500 $DEVFLAG -Wall -Wno-unused-result -Wno-write-strings"
DEFS='-DDATA_DIR="./data" -DHISCORE_DIR="/.formido" -DCONFIG_DIR="/.formido" -DDEFCONFIG_DIR="."'
# sf3500 first: its SDL_mixer.h / SDL_image.h stand in for the real libraries, which the device does not have
# the 2003 sources lean on SDL pulling in the C headers
PRE="-include stdio.h -include stdlib.h -include string.h -include math.h"
INC="-Isf3500 -Iinclude -I$SYSROOT/usr/include/SDL"
mkdir -p "$OBJ"

for f in src/*.cpp sf3500/*.cpp; do
	o="$OBJ/$(basename "$f" .cpp).o"
	if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ -n "$(find include sf3500 -name '*.h*' -newer "$o" -print -quit)" ]; then
		echo "  $f"
		$CXX $FLAGS -std=gnu++14 $PRE $INC $DEFS -c "$f" -o "$o"
	fi
done
$CXX $FLAGS -o "$OUTNAME.mipsel" "$OBJ"/*.o -lSDL -lpthread -ldl -lm
$STRIP -o "$OUTNAME.stripped.mipsel" "$OUTNAME.mipsel"
echo "built $HERE/$OUTNAME.stripped.mipsel"
