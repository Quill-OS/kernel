#!/bin/bash
make distclean
make ARCH=arm CROSS_COMPILE=arm-fsl-linux-gnueabi- mx50_rdp_config
make ARCH=arm CROSS_COMPILE=arm-fsl-linux-gnueabi- |tee make.log
#if [ -f u-boot.bin ];then
#	sudo dd if=u-boot.bin of=u-boot-no-padding.bin bs=512 skip=2
#fi
cp u-boot.bin u-boot_mddr_256.bin
