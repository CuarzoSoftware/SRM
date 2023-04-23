#include <SRMCore.h>
#include <SRMDevice.h>
#include <SRMListener.h>
#include <SRMCrtc.h>
#include <SRMEncoder.h>
#include <SRMPlane.h>
#include <SRMConnector.h>
#include <SRMConnectorMode.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GLES2/gl2.h>

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
    glViewport(0,
               0,
               connector->currentMode()->width(),
               connector->currentMode()->height());

    glClearColor(1.f, 1.f, 1.f, 1.f);

    glClear(GL_COLOR_BUFFER_BIT);

    memset(cursor, 200, sizeof(cursor));

    connector->setCursor(cursor);

    float *phase = (float*)data;
    *phase = 0;

    connector->repaint();
}

static void paintGL(SRMConnector *connector, void *data)
{
    float *phase = (float*)data;
    float cosine = cosf(*phase);
    float sine = sinf(*phase);

    glClearColor(cosine,
                 sine,
                 cosine,
                 1.f);

    glClear(GL_COLOR_BUFFER_BIT);

    *phase += 0.1f;

    Int32 x = ((connector->currentMode()->width() - 64)/2)*(1 + cosine);
    Int32 y = ((connector->currentMode()->height() - 64)/2)*(1 + sine);

    if (*phase >= 2*M_PI)
        *phase -= 2*M_PI;

    connector->setCursorPos(x, y);

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
    fprintf(stdout, "DRM device created: %s\n", device->name());
}

void deviceRemovedEvent(SRMListener *, SRMDevice *device)
{
    fprintf(stdout, "DRM device removed: %s\n", device->name());
}

void connectorPluggedEvent(SRMListener *, SRMConnector *connector)
{
    fprintf(stdout, "DRM connector plugged: %s\n", "HOLA");
}

void connectorUnpluggedEvent(SRMListener *, SRMConnector *connector)
{
    fprintf(stdout, "DRM connector unplugged: %s\n", "HOLA");
}

int main(void)
{
    //setenv("EGL_LOG_LEVEL", "debug", 1);

    SRMCore *srm = SRMCore::createSRM(&srmInterface);

    if (!srm)
    {
        fprintf(stderr, "Failed to initialize SRM.\n");
        return 1;
    }

    // Subscribe to DRM events
    SRMListener *deviceCreatedListener = srm->addDeviceCreatedListener(&deviceCreatedEvent);
    SRMListener *connectorPluggedListener = srm->addConnectorPluggedListener(&connectorPluggedEvent);

    for (SRMDevice *device : srm->devices())
    {
        for (SRMConnector *connector : device->connectors())
        {
            if (connector->connected())
            {
                printf("%s\n", connector->manufacturer());
                float *phase = new float();
                if (!connector->initialize(&connectorInterface, phase))
                {
                    delete phase;
                    fprintf(stderr, "Failed to initialize connector %d\n", connector->id());
                }
            }
        }
    }

    int count = 0;

    while (1)
    {
        // Poll DRM devices/connectors hotplugging events (0 disables timeout)
        srm->processMonitor(1000);

        if (count == 6)
            exit(0);

        count++;
    }

    // Unsuscribe to DRM events
    delete deviceCreatedListener;
    delete connectorPluggedListener;

    return 0;
}
