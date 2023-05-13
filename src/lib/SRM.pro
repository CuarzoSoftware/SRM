TEMPLATE = lib
CONFIG -= qt

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
    SRMBuffer.h \
    SRMConnector.h \
    SRMConnectorMode.h \
    SRMCore.h \
    SRMCrtc.h \
    SRMDevice.h \
    SRMEGL.h \
    SRMEncoder.h \
    SRMFormat.h \
    SRMList.h \
    SRMListener.h \
    SRMLog.h \
    SRMPlane.h \
    SRMTypes.h \
    private/SRMBufferPrivate.h \
    private/SRMConnectorModePrivate.h \
    private/SRMConnectorPrivate.h \
    private/SRMCorePrivate.h \
    private/SRMCrtcPrivate.h \
    private/SRMDevicePrivate.h \
    private/SRMEncoderPrivate.h \
    private/SRMListPrivate.h \
    private/SRMListenerPrivate.h \
    private/SRMPlanePrivate.h \
    private/modes/SRMRenderModeCommon.h \
    private/modes/SRMRenderModeItself.h


SOURCES += \
    SRMBuffer.c \
    SRMConnector.c \
    SRMConnectorMode.c \
    SRMCore.c \
    SRMCrtc.c \
    SRMDevice.c \
    SRMEGL.c \
    SRMEncoder.c \
    SRMFormat.c \
    SRMList.c \
    SRMListener.c \
    SRMLog.c \
    SRMPlane.c \
    SRMTypes.c \
    private/SRMBufferPrivate.c \
    private/SRMConnectorModePrivate.c \
    private/SRMConnectorPrivate.c \
    private/SRMCorePrivate.c \
    private/SRMCrtcPrivate.c \
    private/SRMDevicePrivate.c \
    private/SRMEncoderPrivate.c \
    private/SRMListPrivate.c \
    private/SRMListenerPrivate.c \
    private/SRMPlanePrivate.c \
    private/modes/SRMRenderModeCommon.c \
    private/modes/SRMRenderModeItself.c \
    private/modes/SRMRenderPrivate.c

