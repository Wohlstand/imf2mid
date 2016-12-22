TEMPLATE = app
CONFIG += console
CONFIG -= qt

TARGET = imf2mid
DESTDIR = $$PWD/../bin

QMAKE_CFLAGS += -ansi

SOURCES += \
    ../main.c \
    ../imf2mid.c \
    ../jwHash.c

HEADERS += \
    ../imf2mid.h \
    ../jwHash.h


