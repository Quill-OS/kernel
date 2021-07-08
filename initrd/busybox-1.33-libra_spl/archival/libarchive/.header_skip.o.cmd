cmd_archival/libarchive/header_skip.o := arm-nickel-linux-gnueabihf-gcc -Wp,-MD,archival/libarchive/.header_skip.o.d  -std=gnu99 -Iinclude -Ilibbb  -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DBB_VER='"1.33.1"' -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wunused -Wunused-parameter -Wunused-function -Wunused-value -Wmissing-prototypes -Wmissing-declarations -Wno-format-security -Wdeclaration-after-statement -Wold-style-definition -finline-limit=0 -fno-builtin-strlen -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -static-libgcc -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Os    -DKBUILD_BASENAME='"header_skip"'  -DKBUILD_MODNAME='"header_skip"' -c -o archival/libarchive/header_skip.o archival/libarchive/header_skip.c

deps_archival/libarchive/header_skip.o := \
  archival/libarchive/header_skip.c \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/stdc-predef.h \
  include/libbb.h \
    $(wildcard include/config/feature/shadowpasswds.h) \
    $(wildcard include/config/use/bb/shadow.h) \
    $(wildcard include/config/selinux.h) \
    $(wildcard include/config/feature/utmp.h) \
    $(wildcard include/config/locale/support.h) \
    $(wildcard include/config/use/bb/pwd/grp.h) \
    $(wildcard include/config/lfs.h) \
    $(wildcard include/config/feature/buffers/go/on/stack.h) \
    $(wildcard include/config/feature/buffers/go/in/bss.h) \
    $(wildcard include/config/variable/arch/pagesize.h) \
    $(wildcard include/config/feature/verbose.h) \
    $(wildcard include/config/feature/etc/services.h) \
    $(wildcard include/config/feature/ipv6.h) \
    $(wildcard include/config/feature/seamless/xz.h) \
    $(wildcard include/config/feature/seamless/lzma.h) \
    $(wildcard include/config/feature/seamless/bz2.h) \
    $(wildcard include/config/feature/seamless/gz.h) \
    $(wildcard include/config/feature/seamless/z.h) \
    $(wildcard include/config/float/duration.h) \
    $(wildcard include/config/feature/check/names.h) \
    $(wildcard include/config/feature/prefer/applets.h) \
    $(wildcard include/config/long/opts.h) \
    $(wildcard include/config/feature/pidfile.h) \
    $(wildcard include/config/feature/syslog.h) \
    $(wildcard include/config/feature/syslog/info.h) \
    $(wildcard include/config/warn/simple/msg.h) \
    $(wildcard include/config/feature/individual.h) \
    $(wildcard include/config/ash.h) \
    $(wildcard include/config/sh/is/ash.h) \
    $(wildcard include/config/bash/is/ash.h) \
    $(wildcard include/config/hush.h) \
    $(wildcard include/config/sh/is/hush.h) \
    $(wildcard include/config/bash/is/hush.h) \
    $(wildcard include/config/echo.h) \
    $(wildcard include/config/printf.h) \
    $(wildcard include/config/test.h) \
    $(wildcard include/config/test1.h) \
    $(wildcard include/config/test2.h) \
    $(wildcard include/config/kill.h) \
    $(wildcard include/config/killall.h) \
    $(wildcard include/config/killall5.h) \
    $(wildcard include/config/chown.h) \
    $(wildcard include/config/ls.h) \
    $(wildcard include/config/xxx.h) \
    $(wildcard include/config/route.h) \
    $(wildcard include/config/feature/hwib.h) \
    $(wildcard include/config/desktop.h) \
    $(wildcard include/config/feature/crond/d.h) \
    $(wildcard include/config/feature/setpriv/capabilities.h) \
    $(wildcard include/config/run/init.h) \
    $(wildcard include/config/feature/securetty.h) \
    $(wildcard include/config/pam.h) \
    $(wildcard include/config/use/bb/crypt.h) \
    $(wildcard include/config/feature/adduser/to/group.h) \
    $(wildcard include/config/feature/del/user/from/group.h) \
    $(wildcard include/config/ioctl/hex2str/error.h) \
    $(wildcard include/config/feature/editing.h) \
    $(wildcard include/config/feature/editing/history.h) \
    $(wildcard include/config/feature/tab/completion.h) \
    $(wildcard include/config/shell/ash.h) \
    $(wildcard include/config/shell/hush.h) \
    $(wildcard include/config/feature/editing/savehistory.h) \
    $(wildcard include/config/feature/username/completion.h) \
    $(wildcard include/config/feature/editing/vi.h) \
    $(wildcard include/config/feature/editing/save/on/exit.h) \
    $(wildcard include/config/pmap.h) \
    $(wildcard include/config/feature/show/threads.h) \
    $(wildcard include/config/feature/ps/additional/columns.h) \
    $(wildcard include/config/feature/topmem.h) \
    $(wildcard include/config/feature/top/smp/process.h) \
    $(wildcard include/config/pgrep.h) \
    $(wildcard include/config/pkill.h) \
    $(wildcard include/config/pidof.h) \
    $(wildcard include/config/sestatus.h) \
    $(wildcard include/config/unicode/support.h) \
    $(wildcard include/config/feature/mtab/support.h) \
    $(wildcard include/config/feature/clean/up.h) \
    $(wildcard include/config/feature/devfs.h) \
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
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/ctype.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/xlocale.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/dirent.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/dirent.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/errno.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/errno.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/linux/errno.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm/errno.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm-generic/errno.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm-generic/errno-base.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/fcntl.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/fcntl.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/fcntl-linux.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/uio.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/types.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/time.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/select.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/select.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/sigset.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/time.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/sysmacros.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/pthreadtypes.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/stat.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/inttypes.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/netdb.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/netinet/in.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/socket.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/uio.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/socket.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/socket_type.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/sockaddr.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm/socket.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm-generic/socket.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm/sockios.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm-generic/sockios.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/in.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/rpc/netdb.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/siginfo.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/netdb.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/setjmp.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/setjmp.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/signal.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/signum.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/sigaction.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/sigcontext.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm/sigcontext.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/sigstack.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/ucontext.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/sigthread.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/paths.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/stdio.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/libio.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/_G_config.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/wchar.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/lib/gcc/arm-nickel-linux-gnueabihf/4.9.4/include/stdarg.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/sys_errlist.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/stdlib.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/waitflags.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/waitstatus.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/alloca.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/stdlib-float.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/string.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/libgen.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/poll.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/poll.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/poll.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/ioctl.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/ioctls.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm/ioctls.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm-generic/ioctls.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/linux/ioctl.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm/ioctl.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm-generic/ioctl.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/ioctl-types.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/ttydefaults.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/mman.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/mman.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/mman-linux.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/resource.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/resource.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/stat.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/time.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/wait.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/termios.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/termios.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/timex.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/param.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/param.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/linux/param.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm/param.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/asm-generic/param.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/pwd.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/grp.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/mntent.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/sys/statfs.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/statfs.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/utmp.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/utmp.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/utmpx.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/bits/utmpx.h \
  /run/media/nicolas/b0efede0-edb5-4be4-a03e-ada6e449e2d5/kobo-qemu/Toolchain/nickeltc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include/arpa/inet.h \
  include/pwd_.h \
  include/grp_.h \
  include/shadow_.h \
  include/xatonum.h \
  include/bb_archive.h \
    $(wildcard include/config/feature/tar/uname/gname.h) \
    $(wildcard include/config/feature/tar/long/options.h) \
    $(wildcard include/config/tar.h) \
    $(wildcard include/config/dpkg.h) \
    $(wildcard include/config/dpkg/deb.h) \
    $(wildcard include/config/feature/tar/gnu/extensions.h) \
    $(wildcard include/config/feature/tar/to/command.h) \
    $(wildcard include/config/feature/tar/selinux.h) \
    $(wildcard include/config/cpio.h) \
    $(wildcard include/config/rpm2cpio.h) \
    $(wildcard include/config/rpm.h) \
    $(wildcard include/config/feature/ar/create.h) \
    $(wildcard include/config/feature/ar/long/filenames.h) \
    $(wildcard include/config/zcat.h) \

archival/libarchive/header_skip.o: $(deps_archival/libarchive/header_skip.o)

$(deps_archival/libarchive/header_skip.o):
