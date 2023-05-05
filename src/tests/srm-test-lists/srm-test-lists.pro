TARGET = srm-test-lists

TEMPLATE = app
CONFIG += console c99
CONFIG -= qt

DESTDIR = $$PWD/../../../build
INCLUDEPATH += $$PWD/../../lib
LIBS += -L$$PWD/../../../build -lSRM

QMAKE_LFLAGS += -Wl,-rpath=.

SOURCES += \
    main.c

