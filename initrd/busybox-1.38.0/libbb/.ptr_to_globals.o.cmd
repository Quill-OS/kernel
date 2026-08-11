cmd_libbb/ptr_to_globals.o := arm-kobo-linux-gnueabihf-gcc -Wp,-MD,libbb/.ptr_to_globals.o.d  -std=gnu99 -Iinclude -Ilibbb  -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -DBB_VER='"1.38.0"' -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wunused -Wunused-parameter -Wunused-function -Wunused-value -Wmissing-prototypes -Wmissing-declarations -Wno-format-security -Wdeclaration-after-statement -Wold-style-definition -finline-limit=0 -fno-builtin-strlen -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -static-libgcc -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Oz    -DKBUILD_BASENAME='"ptr_to_globals"'  -DKBUILD_MODNAME='"ptr_to_globals"' -c -o libbb/ptr_to_globals.o libbb/ptr_to_globals.c

deps_libbb/ptr_to_globals.o := \
  libbb/ptr_to_globals.c \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/errno.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/features.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/sys/cdefs.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/wordsize.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/gnu/stubs.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/bits/errno.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/linux/errno.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/asm/errno.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/asm-generic/errno.h \
  /home/nicolas/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot/usr/include/asm-generic/errno-base.h \

libbb/ptr_to_globals.o: $(deps_libbb/ptr_to_globals.o)

$(deps_libbb/ptr_to_globals.o):
