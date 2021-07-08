#configuration
CONFIG +=  cross_compile shared qpa no_mocdepend release qt_no_framework
host_build {
    QT_ARCH = x86_64
    QT_TARGET_ARCH = arm
} else {
    QT_ARCH = arm
}
QT_EDITION = OpenSource
QT_CONFIG +=  minimal-config small-config medium-config large-config full-config evdev linuxfb c++11 accessibility shared qpa reduce_exports reduce_relocations clock-gettime clock-monotonic posix_fallocate mremap getaddrinfo ipv6ifname getifaddrs inotify eventfd system-jpeg system-png png freetype harfbuzz system-zlib dbus dbus-linked openssl icu concurrent audio-backend release

#versioning
QT_VERSION = 5.2.1
QT_MAJOR_VERSION = 5
QT_MINOR_VERSION = 2
QT_PATCH_VERSION = 1

#namespaces
QT_LIBINFIX = 
QT_NAMESPACE = 

# pkgconfig
PKG_CONFIG_SYSROOT_DIR = /.

QMAKE_RPATHDIR +=  "/usr/local/Qt-5.2.1-arm/lib"
QT_GCC_MAJOR_VERSION = 4
QT_GCC_MINOR_VERSION = 9
QT_GCC_PATCH_VERSION = 4
