cd ~/handhelds/sf3000-work/formido
mkdir -p botruns
for tag in nonav nav; do
  : > botruns/stall_$tag.txt
  for seed in 1 2 3 4 5 6 7 8 9 10 11 12; do
    D=botruns/stall_$tag-$seed; rm -rf $D; mkdir -p $D/.formido; ln -s $PWD/data $D/data; cp formido.cfg $D/
    ( cd $D
      if [ $tag = nonav ]; then export FORMIDO_NONAV=1; fi
      HOME=$PWD SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy FORMIDO_BOT=$((seed%4)) FORMIDO_SEED=$seed FORMIDO_FAST=1 timeout 600 ../../formido-native > out.txt 2>&1
      grep -h "^RESULT" out.txt >> ../stall_$tag.txt )
    rm -rf $D
  done
done
