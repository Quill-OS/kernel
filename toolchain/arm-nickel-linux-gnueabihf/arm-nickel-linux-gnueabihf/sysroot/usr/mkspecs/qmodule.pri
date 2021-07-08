CONFIG +=  cross_compile qpa largefile neon pcre
QT_BUILD_PARTS += libs
QT_NO_DEFINES =  ALSA CUPS EGL EGLFS FONTCONFIG GLIB ICONV IMAGEFORMAT_JPEG NIS OPENGL OPENVG PULSEAUDIO STYLE_GTK ZLIB
QT_QCONFIG_PATH = 
host_build {
    QT_CPU_FEATURES.x86_64 =  mmx sse sse2
} else {
    QT_CPU_FEATURES.arm =  neon
}
QT_COORD_TYPE = double
QT_LFLAGS_ODBC   = -lodbc
QMAKE_LFLAGS = -std=c++11 -Wl,--as-needed
styles += mac fusion windows
DEFINES += QT_NO_MTDEV
QT_LIBS_DBUS = -ldbus-1
DEFINES += QT_NO_LIBUDEV
DEFINES += QT_NO_XCB
DEFINES += QT_NO_XKBCOMMON
LIBS +=  -l"z" -l"png" -l"icui18n" -l"icuuc" -l"icudata" -l"dbus-1"
QMAKE_RPATHDIR +=  "/usr/local/Qt-5.2.1-arm/lib"
sql-drivers = 
sql-plugins =  sqlite
