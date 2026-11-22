cd ~/handhelds/sf3000-work/formido
D=botruns/flame
mkdir -p $D/.formido
[ -e $D/data ] || ln -s $PWD/data $D/data
cp formido.cfg $D/
cd $D
FLAME=${FLAME:-}
HOME=$PWD SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy FORMIDO_FAST=1 FORMIDO_BOT=${1:-1} FORMIDO_SEED=${2:-3} FORMIDO_T0=${3:-120} timeout 300 ../../formido-native 2>&1 | grep -E "^flame|^RESULT" | cut -c1-130
