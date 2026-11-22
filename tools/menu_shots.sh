cd ~/handhelds/sf3000-work/formido
D=botruns/menus
mkdir -p $D/.formido
[ -e $D/data ] || ln -s $PWD/data $D/data
cp formido.cfg $D/
find $D -maxdepth 1 -name 'shot_*.bmp' -delete
cd $D
HOME=$PWD SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy FORMIDO_PAD="$2" FORMIDO_SHOT="$1" timeout 25 ../../formido-native >/dev/null 2>&1
ls | grep shot_
