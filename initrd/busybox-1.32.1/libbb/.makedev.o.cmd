cmd_libbb/makedev.o := arm-linux-gnueabihf-gcc -Wp,-MD,libbb/.makedev.o.d   -std=gnu99 -Iinclude -Ilibbb  -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DBB_VER='"1.32.1"'  -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wunused -Wunused-parameter -Wunused-function -Wunused-value -Wmissing-prototypes -Wmissing-declarations -Wno-format-security -Wdeclaration-after-statement -Wold-style-definition -finline-limit=0 -fno-builtin-strlen -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -static-libgcc -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Os     -DKBUILD_BASENAME='"makedev"'  -DKBUILD_MODNAME='"makedev"' -c -o libbb/makedev.o libbb/makedev.c

deps_libbb/makedev.o := \
  libbb/makedev.c \
  include/platform.h \
    $(wildcard include/config/werror.h) \
    $(wildcard include/config/big/endian.h) \
    $(wildcard include/config/little/endian.h) \
    $(wildcard include/config/nommu.h) \
  /home/build/kobo/toolchain/gcc-4.8/lib/gcc/arm-linux-gnueabihf/4.8.1/include-fixed/limits.h \
  /home/build/kobo/toolchain/gcc-4.8/lib/gcc/arm-linux-gnueabihf/4.8.1/include-fixed/syslimits.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/limits.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/features.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/predefs.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/cdefs.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/wordsize.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/gnu/stubs.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/posix1_lim.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/local_lim.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/linux/limits.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/posix2_lim.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/xopen_lim.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/stdio_lim.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/byteswap.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/byteswap.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/endian.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/endian.h \
  /home/build/kobo/toolchain/gcc-4.8/lib/gcc/arm-linux-gnueabihf/4.8.1/include/stdint.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/stdint.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/wchar.h \
  /home/build/kobo/toolchain/gcc-4.8/lib/gcc/arm-linux-gnueabihf/4.8.1/include/stdbool.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/unistd.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/posix_opt.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/environments.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/types.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/typesizes.h \
  /home/build/kobo/toolchain/gcc-4.8/lib/gcc/arm-linux-gnueabihf/4.8.1/include/stddef.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/confname.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/getopt.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/sysmacros.h \

libbb/makedev.o: $(deps_libbb/makedev.o)

$(deps_libbb/makedev.o):
