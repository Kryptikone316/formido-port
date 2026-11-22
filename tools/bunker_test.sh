# SPDX-License-Identifier: GPL-2.0-or-later
cd ~/handhelds/sf3000-work/formido
D=botruns/bk; rm -rf $D; mkdir -p $D/.formido; ln -s $PWD/data $D/data; cp formido.cfg $D/
cd $D
# enter the menu? the bot skips it: FORMIDO_BOT drives menus; here the pad script is used, so start the game via the bot's menu path
HOME=$PWD SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy FORMIDO_BUNKERLOG=1 FORMIDO_BUNKEREXIT=1 FORMIDO_KIT=1 FORMIDO_GOD=1 FORMIDO_SEED=4 FORMIDO_FAST=1 FORMIDO_BOT=9 timeout 120 ../../formido-native 2>&1 | grep -E "outside|inside|RESULT|kit" | cut -c1-110 | head -40
