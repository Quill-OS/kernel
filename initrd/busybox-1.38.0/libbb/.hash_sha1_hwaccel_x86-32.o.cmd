cmd_libbb/hash_sha1_hwaccel_x86-32.o := arm-kobo-linux-gnueabihf-gcc -Wp,-MD,libbb/.hash_sha1_hwaccel_x86-32.o.d  -std=gnu99 -Iinclude -Ilibbb  -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -DBB_VER='"1.38.0"' -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wunused -Wunused-parameter -Wunused-function -Wunused-value -Wmissing-prototypes -Wmissing-declarations -Wno-format-security -Wdeclaration-after-statement -Wold-style-definition -finline-limit=0 -fno-builtin-strlen -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -static-libgcc -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Oz       -c -o libbb/hash_sha1_hwaccel_x86-32.o libbb/hash_sha1_hwaccel_x86-32.S

deps_libbb/hash_sha1_hwaccel_x86-32.o := \
  libbb/hash_sha1_hwaccel_x86-32.S \
    $(wildcard include/config/sha1/hwaccel.h) \

libbb/hash_sha1_hwaccel_x86-32.o: $(deps_libbb/hash_sha1_hwaccel_x86-32.o)

$(deps_libbb/hash_sha1_hwaccel_x86-32.o):
