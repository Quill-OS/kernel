cmd_scripts/kconfig/kxgettext.o := gcc -Wp,-MD,scripts/kconfig/.kxgettext.o.d  -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer       -c -o scripts/kconfig/kxgettext.o scripts/kconfig/kxgettext.c

deps_scripts/kconfig/kxgettext.o := \
  scripts/kconfig/kxgettext.c \
  /usr/include/stdc-predef.h \
  /usr/include/stdlib.h \
  /usr/include/i386-linux-gnu/bits/libc-header-start.h \
  /usr/include/features.h \
  /usr/include/i386-linux-gnu/sys/cdefs.h \
  /usr/include/i386-linux-gnu/bits/wordsize.h \
  /usr/include/i386-linux-gnu/bits/long-double.h \
  /usr/include/i386-linux-gnu/gnu/stubs.h \
  /usr/include/i386-linux-gnu/gnu/stubs-32.h \
  /usr/lib/gcc/i686-linux-gnu/8/include/stddef.h \
  /usr/include/i386-linux-gnu/bits/waitflags.h \
  /usr/include/i386-linux-gnu/bits/waitstatus.h \
  /usr/include/i386-linux-gnu/bits/floatn.h \
  /usr/include/i386-linux-gnu/bits/floatn-common.h \
  /usr/include/i386-linux-gnu/sys/types.h \
  /usr/include/i386-linux-gnu/bits/types.h \
  /usr/include/i386-linux-gnu/bits/typesizes.h \
  /usr/include/i386-linux-gnu/bits/types/clock_t.h \
  /usr/include/i386-linux-gnu/bits/types/clockid_t.h \
  /usr/include/i386-linux-gnu/bits/types/time_t.h \
  /usr/include/i386-linux-gnu/bits/types/timer_t.h \
  /usr/include/i386-linux-gnu/bits/stdint-intn.h \
  /usr/include/endian.h \
  /usr/include/i386-linux-gnu/bits/endian.h \
  /usr/include/i386-linux-gnu/bits/byteswap.h \
  /usr/include/i386-linux-gnu/bits/uintn-identity.h \
  /usr/include/i386-linux-gnu/sys/select.h \
  /usr/include/i386-linux-gnu/bits/select.h \
  /usr/include/i386-linux-gnu/bits/types/sigset_t.h \
  /usr/include/i386-linux-gnu/bits/types/__sigset_t.h \
  /usr/include/i386-linux-gnu/bits/types/struct_timeval.h \
  /usr/include/i386-linux-gnu/bits/types/struct_timespec.h \
  /usr/include/i386-linux-gnu/bits/pthreadtypes.h \
  /usr/include/i386-linux-gnu/bits/thread-shared-types.h \
  /usr/include/i386-linux-gnu/bits/pthreadtypes-arch.h \
  /usr/include/alloca.h \
  /usr/include/i386-linux-gnu/bits/stdlib-bsearch.h \
  /usr/include/i386-linux-gnu/bits/stdlib-float.h \
  /usr/include/string.h \
  /usr/include/i386-linux-gnu/bits/types/locale_t.h \
  /usr/include/i386-linux-gnu/bits/types/__locale_t.h \
  /usr/include/strings.h \
  scripts/kconfig/lkc.h \
  scripts/kconfig/expr.h \
  /usr/include/stdio.h \
  /usr/lib/gcc/i686-linux-gnu/8/include/stdarg.h \
  /usr/include/i386-linux-gnu/bits/types/__fpos_t.h \
  /usr/include/i386-linux-gnu/bits/types/__mbstate_t.h \
  /usr/include/i386-linux-gnu/bits/types/__fpos64_t.h \
  /usr/include/i386-linux-gnu/bits/types/__FILE.h \
  /usr/include/i386-linux-gnu/bits/types/FILE.h \
  /usr/include/i386-linux-gnu/bits/types/struct_FILE.h \
  /usr/include/i386-linux-gnu/bits/stdio_lim.h \
  /usr/include/i386-linux-gnu/bits/sys_errlist.h \
  /usr/include/i386-linux-gnu/bits/stdio.h \
  /usr/lib/gcc/i686-linux-gnu/8/include/stdbool.h \
  /usr/include/libintl.h \
  /usr/include/locale.h \
  /usr/include/i386-linux-gnu/bits/locale.h \
  scripts/kconfig/lkc_proto.h \

scripts/kconfig/kxgettext.o: $(deps_scripts/kconfig/kxgettext.o)

$(deps_scripts/kconfig/kxgettext.o):
