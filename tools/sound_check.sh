# SPDX-License-Identifier: GPL-2.0-or-later
cd ~/handhelds/sf3000-work/formido
SR=$HOME/handhelds/sf3000-work/buildroot/output/host/mipsel-buildroot-linux-gnu/sysroot
D=botruns/snd
mkdir -p $D/.formido
[ -e $D/data ] || ln -s $PWD/data $D/data
cp formido.cfg $D/
cd $D
HOME=$PWD FORMIDO_FAST=1 FORMIDO_FLAME=1 FORMIDO_TYRANT=1 FORMIDO_BOT=1 FORMIDO_SEED=3 FORMIDO_T0=0 timeout 600 qemu-mipsel-static -L "$SR" ../../formido-dev.stripped.mipsel 2>&1 | grep -E "could not load|^flame|^RESULT|tyrant" | cut -c1-120 | head
