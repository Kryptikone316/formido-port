# SPDX-License-Identifier: GPL-2.0-or-later
# usage: shot_test.sh <comma separated ms list> [extra env assignments are taken from the environment]; run on the host
cd ~/handhelds/sf3000-work/formido
D=botruns/shots
mkdir -p $D/.formido
[ -e $D/data ] || ln -s $PWD/data $D/data
cp formido.cfg $D/
find $D -maxdepth 1 -name 'shot_*.bmp' -delete
cd $D
HOME=$PWD SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy FORMIDO_SHOT="$1" timeout ${2:-30} ../../formido-native >/dev/null 2>&1
ls | grep -c shot_
