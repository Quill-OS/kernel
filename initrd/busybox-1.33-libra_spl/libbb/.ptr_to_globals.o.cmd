cmd_libbb/ptr_to_globals.o := arm-nickel-linux-gnueabihf-gcc -Wp,-MD,libbb/.ptr_to_globals.o.d  -std=gnu99 -Iinclude -Ilibbb  -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DBB_VER='"1.33.1"' -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wunused -Wunused-parameter -Wunused-function -Wunused-value -Wmissing-prototypes -Wmissing-declarations -Wno-format-security -Wdeclaration-after-statement -Wold-style-definition -finline-limit=0 -fno-builtin-strlen -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -static-libgcc -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Os    -DKBUILD_BASENAME='"ptr_to_globals"'  -DKBUILD_MODNAME='"ptr_to_globals"' -c -o libbb/ptr_to_globals.o libbb/ptr_to_globals.c

deps_libbb/ptr_to_globals.o := \
  libbb/ptr_to_globals.c \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/stdc-predef.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/errno.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/features.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/cdefs.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/wordsize.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/gnu/stubs.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/gnu/stubs-hard.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/errno.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/linux/errno.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm/errno.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm-generic/errno.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm-generic/errno-base.h \

libbb/ptr_to_globals.o: $(deps_libbb/ptr_to_globals.o)

$(deps_libbb/ptr_to_globals.o):
