cmd_applets/applets.o := armv7l-linux-musleabihf-gcc -Wp,-MD,applets/.applets.o.d  -std=gnu99 -Iinclude -Ilibbb  -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DBB_VER='"1.34.1"' -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wunused -Wunused-parameter -Wunused-function -Wunused-value -Wmissing-prototypes -Wmissing-declarations -Wno-format-security -Wdeclaration-after-statement -Wold-style-definition -finline-limit=0 -fno-builtin-strlen -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -static-libgcc -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Os    -DKBUILD_BASENAME='"applets"'  -DKBUILD_MODNAME='"applets"' -c -o applets/applets.o applets/applets.c

deps_applets/applets.o := \
  applets/applets.c \
    $(wildcard include/config/build/libbusybox.h) \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/stdc-predef.h \
  include/busybox.h \
    $(wildcard include/config/feature/prefer/applets.h) \
    $(wildcard include/config/feature/sh/standalone.h) \
    $(wildcard include/config/feature/sh/nofork.h) \
    $(wildcard include/config/feature/suid.h) \
    $(wildcard include/config/feature/installer.h) \
    $(wildcard include/config/feature/shared/busybox.h) \
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
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/ctype.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/dirent.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/dirent.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/errno.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/errno.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/fcntl.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/fcntl.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/inttypes.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/netdb.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/netinet/in.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/sys/socket.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/socket.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/setjmp.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/setjmp.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/signal.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/signal.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/paths.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/stdio.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/stdlib.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/alloca.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/stdarg.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/stddef.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/string.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/strings.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/libgen.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/poll.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/poll.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/sys/ioctl.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/ioctl.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/ioctl_fix.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/sys/mman.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/mman.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/sys/resource.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/sys/time.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/sys/select.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/resource.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/sys/stat.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/stat.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/sys/types.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/sys/sysmacros.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/sys/wait.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/termios.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/termios.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/time.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/sys/param.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/pwd.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/grp.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/mntent.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/sys/statfs.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/sys/statvfs.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/bits/statfs.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/utmp.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/utmpx.h \
  /home/build/inkbox/kernel/toolchain/armv7l-linux-musleabihf-cross/armv7l-linux-musleabihf/include/arpa/inet.h \
  include/pwd_.h \
  include/grp_.h \
  include/shadow_.h \
  include/xatonum.h \
  include/applet_metadata.h \
    $(wildcard include/config/install/no/usr.h) \

applets/applets.o: $(deps_applets/applets.o)

$(deps_applets/applets.o):
