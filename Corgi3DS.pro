QT += core gui multimedia
greaterThan(QT_MAJOR_VERSION, 4) : QT += widgets

TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle

QMAKE_CFLAGS_RELEASE -= -O
QMAKE_CFLAGS_RELEASE -= -O1
QMAKE_CFLAGS_RELEASE *= -O2
QMAKE_CFLAGS_RELEASE -= -O3

SOURCES += src/qt/main.cpp \
    src/core/emulator.cpp \
    src/core/cpu/arm.cpp \
    src/core/cpu/arm_interpret.cpp \
    src/core/cpu/arm_disasm.cpp \
    src/core/cpu/cp15.cpp \
    src/core/cpu/thumb_disasm.cpp \
    src/core/cpu/thumb_interpret.cpp \
    src/core/arm9/rsa9.cpp \
    src/core/timers.cpp \
    src/core/arm9/dma9.cpp \
    src/core/pxi.cpp \
    src/core/arm11/mpcore_pmr.cpp \
    src/core/arm11/gpu.cpp \
    src/core/arm9/aes9.cpp \
    src/core/arm9/sha9.cpp \
    src/core/common/bswp.cpp \
    src/core/common/rotr.cpp \
    src/core/arm9/aes_lib.c \
    src/core/arm9/emmc.cpp \
    src/core/arm9/interrupt9.cpp \
    src/qt/emuwindow.cpp \
    src/core/i2c.cpp \
    src/core/common/exceptions.cpp \
    src/polarssl/aes.c \
    src/polarssl/bignum.c \
    src/polarssl/rsa.c \
    src/polarssl/sha1.c \
    src/polarssl/sha2.c

HEADERS += \
    src/core/emulator.hpp \
    src/core/cpu/arm.hpp \
    src/core/cpu/arm_disasm.hpp \
    src/core/cpu/arm_interpret.hpp \
    src/core/common/rotr.hpp \
    src/core/cpu/cp15.hpp \
    src/core/arm9/rsa9.hpp \
    src/core/timers.hpp \
    src/core/arm9/dma9.hpp \
    src/core/pxi.hpp \
    src/core/arm11/mpcore_pmr.hpp \
    src/core/arm11/gpu.hpp \
    src/core/arm9/aes9.hpp \
    src/core/arm9/sha9.hpp \
    src/core/common/bswp.hpp \
    src/core/arm9/aes_lib.hpp \
    src/core/arm9/aes_lib.h \
    src/core/arm9/emmc.hpp \
    src/core/arm9/interrupt9.hpp \
    src/qt/emuwindow.hpp \
    src/core/i2c.hpp \
    src/core/common/common.hpp \
    src/core/common/exceptions.hpp \
    src/polarssl/aes.h \
    src/polarssl/bignum.h \
    src/polarssl/bn_mul.h \
    src/polarssl/config.h \
    src/polarssl/padlock.h \
    src/polarssl/rsa.h \
    src/polarssl/sha1.h \
    src/polarssl/sha2.h

INCLUDEPATH += /usr/local/include

LIBS += -L/usr/local/lib
