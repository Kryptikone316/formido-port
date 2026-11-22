cd ~/handhelds/sf3000-work/formido
SR=$HOME/handhelds/sf3000-work/buildroot/output/host/mipsel-buildroot-linux-gnu/sysroot
D=botruns/oneq; rm -rf $D; mkdir -p $D/.formido; ln -s $PWD/data $D/data; cp formido.cfg $D/
cd $D; HOME=$PWD FORMIDO_BOT=${2:-1} FORMIDO_SEED=$1 FORMIDO_FAST=1 timeout 900 qemu-mipsel-static -L "$SR" ../../formido-dev.stripped.mipsel 2>&1 | grep -E "^(RESULT|nav)" | cut -c1-110
