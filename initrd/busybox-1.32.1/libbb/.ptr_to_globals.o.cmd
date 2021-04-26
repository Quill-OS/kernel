cmd_libbb/ptr_to_globals.o := arm-linux-gnueabihf-gcc -Wp,-MD,libbb/.ptr_to_globals.o.d   -std=gnu99 -Iinclude -Ilibbb  -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DBB_VER='"1.32.1"'  -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wunused -Wunused-parameter -Wunused-function -Wunused-value -Wmissing-prototypes -Wmissing-declarations -Wno-format-security -Wdeclaration-after-statement -Wold-style-definition -finline-limit=0 -fno-builtin-strlen -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -static-libgcc -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Os     -DKBUILD_BASENAME='"ptr_to_globals"'  -DKBUILD_MODNAME='"ptr_to_globals"' -c -o libbb/ptr_to_globals.o libbb/ptr_to_globals.c

deps_libbb/ptr_to_globals.o := \
  libbb/ptr_to_globals.c \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/errno.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/features.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/predefs.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/cdefs.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/wordsize.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/gnu/stubs.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/errno.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/linux/errno.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/asm/errno.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/asm-generic/errno.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/asm-generic/errno-base.h \

libbb/ptr_to_globals.o: $(deps_libbb/ptr_to_globals.o)

$(deps_libbb/ptr_to_globals.o):
