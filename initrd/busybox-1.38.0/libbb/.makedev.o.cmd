cmd_libbb/makedev.o := arm-kobo-linux-gnueabihf-gcc -Wp,-MD,libbb/.makedev.o.d  -std=gnu99 -Iinclude -Ilibbb  -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -DBB_VER='"1.38.0"' -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wunused -Wunused-parameter -Wunused-function -Wunused-value -Wmissing-prototypes -Wmissing-declarations -Wno-format-security -Wdeclaration-after-statement -Wold-style-definition -finline-limit=0 -fno-builtin-strlen -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -static-libgcc -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Oz    -DKBUILD_BASENAME='"makedev"'  -DKBUILD_MODNAME='"makedev"' -c -o libbb/makedev.o libbb/makedev.c

deps_libbb/makedev.o := \
  libbb/makedev.c \
  include/platform.h \
    $(wildcard include/config/werror.h) \
    $(wildcard include/config/big/endian.h) \
    $(wildcard include/config/little/endian.h) \
    $(wildcard include/config/nommu.h) \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/lib/gcc/arm-kobo-linux-gnueabihf/14.4.0/include/limits.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/lib/gcc/arm-kobo-linux-gnueabihf/14.4.0/include/syslimits.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/limits.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/features.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/sys/cdefs.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/wordsize.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/gnu/stubs.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/posix1_lim.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/local_lim.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/linux/limits.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/posix2_lim.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/xopen_lim.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/stdio_lim.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/byteswap.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/byteswap.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/endian.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/endian.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/lib/gcc/arm-kobo-linux-gnueabihf/14.4.0/include/stdint.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/stdint.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/wchar.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/lib/gcc/arm-kobo-linux-gnueabihf/14.4.0/include/stdbool.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/unistd.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/posix_opt.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/environments.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/types.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/typesizes.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/lib/gcc/arm-kobo-linux-gnueabihf/14.4.0/include/stddef.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/confname.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/getopt.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/sys/sysmacros.h \

libbb/makedev.o: $(deps_libbb/makedev.o)

$(deps_libbb/makedev.o):
