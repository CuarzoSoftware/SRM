TARGET = srm-display-info

TEMPLATE = app
CONFIG += console c++11
CONFIG -= qt

DESTDIR = $$PWD/../../../build

QMAKE_CXXFLAGS_DEBUG *= -O
QMAKE_CXXFLAGS_RELEASE -= -O
QMAKE_CXXFLAGS_RELEASE -= -O1
QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE *= -O3

#LIBS += -L/usr/local/lib/x86_64-linux-gnu -lEGL -lGL -lGLESv2 -linput -ludev -ldrm -lgbm -lrt
LIBS += -L$$PWD/../../../build -lSRM
INCLUDEPATH += ../../lib/ /usr/include/libdrm


QMAKE_LFLAGS += -Wl,-rpath=.

SOURCES += \
    main.cpp

