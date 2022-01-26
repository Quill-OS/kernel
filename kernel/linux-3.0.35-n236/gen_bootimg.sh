#! /bin/sh
./mkbootimg --kernel arch/arm/boot/zImage --ramdisk uramdisk.img --base 0x80800000 --cmdline "console=ttymxc0,115200 init=/init androidboot.console=ttymxc0 max17135:pass=2, fbmem=6M video=mxcepdcfb:E060SCM,bpp=16 no_console_suspend" --board evk_6sl_eink -o boot.img
