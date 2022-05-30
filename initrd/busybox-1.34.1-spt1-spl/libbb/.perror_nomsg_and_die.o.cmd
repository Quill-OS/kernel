cmd_libbb/perror_nomsg_and_die.o := armv7l-linux-musleabihf-gcc -Wp,-MD,libbb/.perror_nomsg_and_die.o.d  -std=gnu99 -Iinclude -Ilibbb  -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DBB_VER='"1.34.1"' -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wunused -Wunused-parameter -Wunused-function -Wunused-value -Wmissing-prototypes -Wmissing-declarations -Wno-format-security -Wdeclaration-after-statement -Wold-style-definition -finline-limit=0 -fno-builtin-strlen -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -static-libgcc -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Os    -DKBUILD_BASENAME='"perror_nomsg_and_die"'  -DKBUILD_MODNAME='"perror_nomsg_and_die"' -c -o libbb/perror_nomsg_and_die.o libbb/perror_nomsg_and_die.c

deps_libbb/perror_nomsg_and_die.o := \
  libbb/perror_nomsg_and_die.c \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/stdc-predef.h \
  include/platform.h \
    $(wildcard include/config/werror.h) \
    $(wildcard include/config/big/endian.h) \
    $(wildcard include/config/little/endian.h) \
    $(wildcard include/config/nommu.h) \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/limits.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/features.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/alltypes.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/limits.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/byteswap.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/stdint.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/stdint.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/endian.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/stdbool.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/unistd.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/posix.h \

libbb/perror_nomsg_and_die.o: $(deps_libbb/perror_nomsg_and_die.o)

$(deps_libbb/perror_nomsg_and_die.o):
