TEMPLATE = app
CONFIG += console
CONFIG -= qt

TARGET = imf2mid
DESTDIR = $$PWD/bin

SOURCES += \
    imf2mid.c \
    converter.c

HEADERS += \
    converter.h


