#!/bin/sh
# Native (x86-64) test build of the game with the test hooks, for bot runs on the host (much faster than QEMU).
# Not for the device. Needs the host's libSDL-1.2 and the SDL headers from the cross sysroot.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SYSROOT=${SYSROOT:-$HOME/handhelds/sf3000-work/buildroot/output/host/mipsel-buildroot-linux-gnu/sysroot}
SDLLIB=${SDLLIB:-/usr/lib/x86_64-linux-gnu/libSDL-1.2.so.0}
cd "$HERE"
mkdir -p obj-native
FLAGS="-O2 -DSF3500 -DFORMIDO_TESTHOOKS -Wall -Wno-unused-result -Wno-write-strings -Wno-format-overflow -Wno-char-subscripts -std=gnu++14"
DEFS='-DDATA_DIR="./data" -DHISCORE_DIR="/.formido" -DCONFIG_DIR="/.formido" -DDEFCONFIG_DIR="."'
PRE="-include stdio.h -include stdlib.h -include string.h -include math.h"
INC="-Isf3500 -Iinclude -I$SYSROOT/usr/include/SDL"
for f in src/*.cpp sf3500/*.cpp; do
	o="obj-native/$(basename "$f" .cpp).o"
	if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ -n "$(find include sf3500 -name '*.h*' -newer "$o" -print -quit)" ]; then
		g++ $FLAGS $PRE $INC $DEFS -c "$f" -o "$o"
	fi
done
g++ -o formido-native obj-native/*.o "$SDLLIB" -lpthread -ldl -lm
echo "built $HERE/formido-native"
