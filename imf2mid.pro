TEMPLATE = app
CONFIG += console
CONFIG -= qt

TARGET = imf2mid
DESTDIR = $$PWD/bin

SOURCES += \
    main.c \
    imf2mid.c

HEADERS += \
    imf2mid.h


