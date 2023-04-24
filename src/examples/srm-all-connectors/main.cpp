#include <SRMCore.h>
#include <SRMDevice.h>
#include <SRMListener.h>
#include <SRMCrtc.h>
#include <SRMEncoder.h>
#include <SRMPlane.h>
#include <SRMConnector.h>
#include <SRMConnectorMode.h>
#include <SRMLog.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GLES2/gl2.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMConnectorPrivate.h>
using namespace SRM;

UInt8 cursor[64*64*4];

static int openRestricted(const char *path, int flags, void *)
{
    // Here libseat could be used instead
    return open(path, flags);
}

static void closeRestricted(int fd, void *)
{
    close(fd);
}

SRMInterface srmInterface
{
    .openRestricted = &openRestricted,
    .closeRestricted = &closeRestricted
};

static void initializeGL(SRMConnector *connector, void *data)
{
    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_BUFFER_BIT);

    glViewport(0,
               0,
               connector->currentMode()->width(),
               connector->currentMode()->height());

    glScissor(0,
              connector->currentMode()->height()/2,
              connector->currentMode()->width()/2,
              connector->currentMode()->height()/2);
    glClearColor(1.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glScissor(connector->currentMode()->width()/2,
              connector->currentMode()->height()/2,
              connector->currentMode()->width()/2,
              connector->currentMode()->height()/2);
    glClearColor(0.f, 1.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glScissor(0,
              0,
              connector->currentMode()->width()/2,
              connector->currentMode()->height()/2);
    glClearColor(0.f, 0.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glScissor(connector->currentMode()->width()/2,
              0,
              connector->currentMode()->width()/2,
              connector->currentMode()->height()/2);
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);


    glScissor(0,
              0,
              connector->currentMode()->width(),
              connector->currentMode()->height());

    memset(cursor, 200, sizeof(cursor));

    //connector->setCursor(cursor);

    float *phase = (float*)data;
    *phase = 0;

    connector->repaint();
}

static void paintGL(SRMConnector *connector, void *data)
{
    float *phase = (float*)data;
    float cosine = cosf(*phase);
    float sine = sinf(*phase);

    glScissor(0,
              0,
              connector->currentMode()->width(),
              connector->currentMode()->height());

    glClearColor(0.f,
                 0.f,
                 0.f,
                 1.f);

    glClear(GL_COLOR_BUFFER_BIT);

    glScissor(connector->currentMode()->width()*(1.f+cosine)/2,
              0,
              1,
              connector->currentMode()->height());

    glClearColor(1.f,
                 1.f,
                 1.f,
                 1.f);

    glClear(GL_COLOR_BUFFER_BIT);

    glScissor(0,
              100,
              connector->currentMode()->width(),
              1);

    glClearColor(1.f,
                 1.f,
                 1.f,
                 1.f);

    glClear(GL_COLOR_BUFFER_BIT);

    *phase += 0.05f;

    Int32 x = ((connector->currentMode()->width() - 64)/2)*(1 + cosine);
    Int32 y = ((connector->currentMode()->height() - 64)/2)*(1 + sine);

    if (*phase >= 2*M_PI)
        *phase -= 2*M_PI;

    //connector->setCursorPos(x, y);

    connector->repaint();
}

static void resizeGL(SRMConnector *connector, void *)
{
    glViewport(0,
               0,
               connector->currentMode()->width(),
               connector->currentMode()->height());

    connector->repaint();
}

static void uninitializeGL(SRMConnector *, void *)
{

}

SRMConnectorInterface connectorInterface
{
    .initializeGL = &initializeGL,
    .paintGL = &paintGL,
    .resizeGL = &resizeGL,
    .uninitializeGL = &uninitializeGL
};

void deviceCreatedEvent(SRMListener *, SRMDevice *device)
{
    SRMLog::log("DRM device created: %s.", device->name());
}

void deviceRemovedEvent(SRMListener *, SRMDevice *device)
{
    SRMLog::log("DRM device removed: %s.", device->name());
}

void connectorPluggedEvent(SRMListener *, SRMConnector *connector)
{
    SRMLog::log("DRM connector plugged: %s.", "HOLA");
}

void connectorUnpluggedEvent(SRMListener *, SRMConnector *connector)
{
    SRMLog::log("DRM connector unplugged: %s.", "HOLA");
}

int main(void)
{
    /* setenv("EGL_LOG_LEVEL", "debug", 1); */

    setenv("SRM_DEBUG", "4", 1);

    SRMCore *srm = SRMCore::createSRM(&srmInterface);

    if (!srm)
    {
        SRMLog::fatal("Failed to initialize SRM.");
        return 1;
    }

    // Subscribe to DRM events
    SRMListener *deviceCreatedListener = srm->addDeviceCreatedListener(&deviceCreatedEvent);
    SRMListener *connectorPluggedListener = srm->addConnectorPluggedListener(&connectorPluggedEvent);

    for (SRMDevice *device : srm->devices())
    {
        for (SRMConnector *connector : device->connectors())
        {
            if (connector->connected())// && connector->id() == 42)
            {
                SRMLog::log("Connector: %s.", connector->manufacturer());

                float *phase = new float();
                if (!connector->initialize(&connectorInterface, phase))
                {
                    delete phase;
                    SRMLog::error("Failed to initialize connector %d.", connector->id());
                }
            }
        }
    }

    usleep(5000000);
    printf("DIE\n");
    abort();

    while (1)
    {
        // Poll DRM devices/connectors hotplugging events (0 disables timeout)
        srm->processMonitor(-1);
    }

    // Unsuscribe to DRM events
    delete deviceCreatedListener;
    delete connectorPluggedListener;

    return 0;
}
