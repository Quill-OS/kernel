cmd_libbb/makedev.o := arm-nickel-linux-gnueabihf-gcc -Wp,-MD,libbb/.makedev.o.d  -std=gnu99 -Iinclude -Ilibbb  -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DBB_VER='"1.33.1"' -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wunused -Wunused-parameter -Wunused-function -Wunused-value -Wmissing-prototypes -Wmissing-declarations -Wno-format-security -Wdeclaration-after-statement -Wold-style-definition -finline-limit=0 -fno-builtin-strlen -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -static-libgcc -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Os    -DKBUILD_BASENAME='"makedev"'  -DKBUILD_MODNAME='"makedev"' -c -o libbb/makedev.o libbb/makedev.c

deps_libbb/makedev.o := \
  libbb/makedev.c \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/stdc-predef.h \
  include/platform.h \
    $(wildcard include/config/werror.h) \
    $(wildcard include/config/big/endian.h) \
    $(wildcard include/config/little/endian.h) \
    $(wildcard include/config/nommu.h) \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/lib/gcc/arm-nickel-linux-gnueabihf/4.9.4/include-fixed/limits.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/lib/gcc/arm-nickel-linux-gnueabihf/4.9.4/include-fixed/syslimits.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/limits.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/features.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/cdefs.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/wordsize.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/gnu/stubs.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/gnu/stubs-hard.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/posix1_lim.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/local_lim.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/linux/limits.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/posix2_lim.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/xopen_lim.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/stdio_lim.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/byteswap.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/byteswap.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/types.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/typesizes.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/byteswap-16.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/endian.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/endian.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/lib/gcc/arm-nickel-linux-gnueabihf/4.9.4/include/stdint.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/stdint.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/wchar.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/lib/gcc/arm-nickel-linux-gnueabihf/4.9.4/include/stdbool.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/unistd.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/posix_opt.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/environments.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/lib/gcc/arm-nickel-linux-gnueabihf/4.9.4/include/stddef.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/confname.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/getopt.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/sysmacros.h \

libbb/makedev.o: $(deps_libbb/makedev.o)

$(deps_libbb/makedev.o):
