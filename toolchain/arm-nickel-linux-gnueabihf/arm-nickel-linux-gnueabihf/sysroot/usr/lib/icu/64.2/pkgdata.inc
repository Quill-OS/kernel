GENCCODE_ASSEMBLY_TYPE=-a gcc
SO=so
SOBJ=so
A=a
LIBPREFIX=lib
LIB_EXT_ORDER=.64.2
COMPILE=arm-nickel-linux-gnueabihf-gcc -D_REENTRANT  -DU_HAVE_ELF_H=1 -DU_HAVE_STRTOD_L=1 -DU_HAVE_XLOCALE_H=1  -DU_ATTRIBUTE_DEPRECATED= -O3 -ffast-math -march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=hard -mthumb  -pipe -fomit-frame-pointer -frename-registers -fweb -std=c11 -Wall -pedantic -Wshadow -Wpointer-arith -Wmissing-prototypes -Wwrite-strings   -c
LIBFLAGS=-I/tc/x-tools/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/include -DPIC -fPIC
GENLIB=arm-nickel-linux-gnueabihf-gcc -O3 -ffast-math -march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=hard -mthumb  -pipe -fomit-frame-pointer -frename-registers -fweb -std=c11 -Wall -pedantic -Wshadow -Wpointer-arith -Wmissing-prototypes -Wwrite-strings   -Wl,-O1 -Wl,--as-needed  -shared -Wl,-Bsymbolic
LDICUDTFLAGS=
LD_SONAME=-Wl,-soname -Wl,
RPATH_FLAGS=
BIR_LDFLAGS=-Wl,-Bsymbolic
AR=arm-nickel-linux-gnueabihf-gcc-ar
ARFLAGS=r
RANLIB=arm-nickel-linux-gnueabihf-gcc-ranlib
INSTALL_CMD=/usr/bin/install -c
