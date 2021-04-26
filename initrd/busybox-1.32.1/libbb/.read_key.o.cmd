cmd_libbb/read_key.o := arm-linux-gnueabihf-gcc -Wp,-MD,libbb/.read_key.o.d   -std=gnu99 -Iinclude -Ilibbb  -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DBB_VER='"1.32.1"'  -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wunused -Wunused-parameter -Wunused-function -Wunused-value -Wmissing-prototypes -Wmissing-declarations -Wno-format-security -Wdeclaration-after-statement -Wold-style-definition -finline-limit=0 -fno-builtin-strlen -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -static-libgcc -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Os     -DKBUILD_BASENAME='"read_key"'  -DKBUILD_MODNAME='"read_key"' -c -o libbb/read_key.o libbb/read_key.c

deps_libbb/read_key.o := \
  libbb/read_key.c \
    $(wildcard include/config/feature/editing/ask/terminal.h) \
    $(wildcard include/config/feature/vi/ask/terminal.h) \
    $(wildcard include/config/feature/less/ask/terminal.h) \
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
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/ctype.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/xlocale.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/dirent.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/dirent.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/errno.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/errno.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/linux/errno.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/asm/errno.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/asm-generic/errno.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/asm-generic/errno-base.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/fcntl.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/fcntl.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/types.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/time.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/select.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/select.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/sigset.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/time.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/sysmacros.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/pthreadtypes.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/uio.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/stat.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/inttypes.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/netdb.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/netinet/in.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/socket.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/uio.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/socket.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/sockaddr.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/asm/socket.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/asm/sockios.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/in.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/rpc/netdb.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/siginfo.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/netdb.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/setjmp.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/setjmp.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/signal.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/signum.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/sigaction.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/sigcontext.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/asm/sigcontext.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/sigstack.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/ucontext.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/sigthread.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/paths.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/stdio.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/libio.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/_G_config.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/wchar.h \
  /home/build/kobo/toolchain/gcc-4.8/lib/gcc/arm-linux-gnueabihf/4.8.1/include/stdarg.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/sys_errlist.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/stdlib.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/waitflags.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/waitstatus.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/alloca.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/string.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/libgen.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/poll.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/poll.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/poll.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/ioctl.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/ioctls.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/asm/ioctls.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/asm-generic/ioctls.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/linux/ioctl.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/asm/ioctl.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/asm-generic/ioctl.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/ioctl-types.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/ttydefaults.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/mman.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/mman.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/resource.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/resource.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/stat.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/time.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/wait.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/termios.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/termios.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/timex.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/param.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/linux/param.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/asm/param.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/pwd.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/grp.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/mntent.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/sys/statfs.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/statfs.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/utmp.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/utmp.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/utmpx.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf/bits/utmpx.h \
  /home/build/kobo/toolchain/gcc-4.8/arm-linux-gnueabihf/libc/usr/include/arpa/inet.h \
  include/pwd_.h \
  include/grp_.h \
  include/shadow_.h \
  include/xatonum.h \

libbb/read_key.o: $(deps_libbb/read_key.o)

$(deps_libbb/read_key.o):
