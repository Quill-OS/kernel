#!/bin/bash

cd board/freescale/mx50_rdp
rm flash_header.S;sync;sleep 1
ln -s flash_header-LPDDR2.S flash_header.S
cd - 

make distclean
make ARCH=arm CROSS_COMPILE=/opt/freescale/usr/local/gcc-4.4.4-glibc-2.11.1-multilib-1.0/arm-fsl-linux-gnueabi/bin/arm-fsl-linux-gnueabi- mx50_rdp_lpddr2_256_config
make ARCH=arm CROSS_COMPILE=/opt/freescale/usr/local/gcc-4.4.4-glibc-2.11.1-multilib-1.0/arm-fsl-linux-gnueabi/bin/arm-fsl-linux-gnueabi-
#if [ -f u-boot.bin ];then
#	sudo dd if=u-boot.bin of=u-boot-no-padding.bin bs=512 skip=2
#fi
cp u-boot.bin u-boot_lpddr2_256.bin

