TEMPLATE = lib

CONFIG -= qt
CONFIG += c++11

DESTDIR = $$PWD/../../build

LIBS += -L/usr/local/lib/x86_64-linux-gnu -lEGL -lGL -lGLESv2 -linput -ludev -ldrm -lgbm -lrt
LIBS += -L/usr/local/lib/x86_64-linux-gnu -ldisplay-info
LIBS += -L/lib/x86_64-linux-gnu -lseat
INCLUDEPATH += /usr/include/libdrm ./core /usr/local/include


# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    SRMConnector.h \
    SRMConnectorMode.h \
    SRMCore.h \
    SRMCrtc.h \
    SRMDevice.h \
    SRMEncoder.h \
    SRMListener.h \
    SRMLog.h \
    SRMNamespaces.h \
    SRMPlane.h \
    private/SRMConnectorModePrivate.h \
    private/SRMConnectorPrivate.h \
    private/SRMCorePrivate.h \
    private/SRMCrtcPrivate.h \
    private/SRMDevicePrivate.h \
    private/SRMEncoderPrivate.h \
    private/SRMListenerPrivate.h \
    private/SRMPlanePrivate.h

SOURCES += \
    SRMConnector.cpp \
    SRMConnectorMode.cpp \
    SRMCore.cpp \
    SRMCrtc.cpp \
    SRMDevice.cpp \
    SRMEncoder.cpp \
    SRMListener.cpp \
    SRMLog.cpp \
    SRMNamespaces.cpp \
    SRMPlane.cpp \
    private/SRMConnectorModePrivate.cpp \
    private/SRMConnectorPrivate.cpp \
    private/SRMCorePrivate.cpp \
    private/SRMCrtcPrivate.cpp \
    private/SRMDevicePrivate.cpp \
    private/SRMEncoderPrivate.cpp \
    private/SRMPlanePrivate.cpp \
    private/SRMRenderPrivate.cpp
