TARGET = srm-display-info

TEMPLATE = app
CONFIG += console
CONFIG -= qt

DESTDIR = $$PWD/../../../build

LIBS += -L$$PWD/../../../build -lSRM
INCLUDEPATH += ../../lib/ /usr/include/libdrm


QMAKE_LFLAGS += -Wl,-rpath=.

SOURCES += \
    main.c

