#!/bin/bash
# Warning! This script is not intended to be ran alone. Instead, it is executed along by the build_all.sh script.

echo "---- Building U-Boot 2009.08-inkbox ----"

cd $TOOLCHAINDIR/bin
export PATH=$PATH:$PWD
cd - &> /dev/null

cd $GITDIR/bootloader/board/freescale/mx50_rdp

rm flash_header.S && sync && sleep 1
ln -s flash_header-20120622_FSL_RAM_PARMS_DSadd2.S flash_header.S

cd - &>/dev/null
cd $GITDIR/bootloader

make ARCH=arm CROSS_COMPILE=$TARGET- -j$THREADS distclean
make ARCH=arm CROSS_COMPILE=$TARGET- -j$THREADS mx50_rdp_config
make ARCH=arm CROSS_COMPILE=$TARGET- -j$THREADS

cp u-boot.bin $GITDIR/kernel/out/u-boot.bin

cd - &>/dev/null

echo "---- Done. Output was saved in $GITDIR/kernel/out/u-boot.bin ----"
