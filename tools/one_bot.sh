# usage: one_bot.sh <seed> [skill]  (run on the host)
cd ~/handhelds/sf3000-work/formido
D=botruns/one; rm -rf $D; mkdir -p $D/.formido; ln -s $PWD/data $D/data; cp formido.cfg $D/
cd $D; HOME=$PWD SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy FORMIDO_BOT=${2:-1} FORMIDO_SEED=$1 FORMIDO_FAST=1 timeout 600 ../../formido-native 2>&1 | grep -E "^(RESULT|nav)" | cut -c1-100
