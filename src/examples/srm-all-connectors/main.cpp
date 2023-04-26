/*
 * Proj: srm-all-connectors example
 *
 * Auth: Eduardo Hopperdietzel
 *
 * Desc: This example renders to all
 *       avaliable connectors at a time
 *       for a few seconds.
 */

#include <SRMCore.h>
#include <SRMDevice.h>
#include <SRMListener.h>
#include <SRMCrtc.h>
#include <SRMEncoder.h>
#include <SRMPlane.h>
#include <SRMConnector.h>
#include <SRMConnectorMode.h>
#include <SRMLog.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

using namespace SRM;

UInt8 cursor[64*64*4];

static int openRestricted(const char *path, int flags, void *)
{
    // Here something like libseat could be used instead
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

static void drawColorSquares(UInt32 w, UInt32 h)
{
    // Red TL square
    glScissor(0, h/2, w/2, h/2);
    glClearColor(1.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Green TR square
    glScissor(w/2, h/2, w/2, h/2);
    glClearColor(0.f, 1.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Blue BL square
    glScissor(0, 0, w/2, h/2);
    glClearColor(0.f, 0.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // White BR square
    glScissor(w/2, 0, w/2, h/2);
    glClearColor(1.f, 1.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

static void initializeGL(SRMConnector *connector, void *data)
{
    UInt32 w = connector->currentMode()->width();
    UInt32 h = connector->currentMode()->width();

    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, w, h);

    // Draw color squares
    drawColorSquares(w, h);

    // Create a gray cursor
    memset(cursor, 200, sizeof(cursor));
    connector->setCursor(cursor);

    float *phase = (float*)data;
    *phase = 0;

    connector->repaint();
}

static void paintGL(SRMConnector *connector, void *data)
{
    UInt32 w = connector->currentMode()->width();
    UInt32 h = connector->currentMode()->height();
    float *phase = (float*)data;
    float cosine = cosf(*phase);
    float sine = sinf(*phase);

    // Draw color squares
    drawColorSquares(w, h);

    // Moving black vertical line
    glScissor((w-5)*(1.f+cosine)/2, 0, 10, h);
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Update cursor pos
    Int32 x = ((w - 64)/2)*(1 + cosine);
    Int32 y = ((h - 64)/2)*(1 + sine);
    connector->setCursorPos(x, y);

    // Increase phase
    *phase += 0.015f;

    if (*phase >= 2*M_PI)
        *phase -= 2*M_PI;

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
    SRMLog::debug("DRM device (GPU) created: %s.", device->name());
}

void deviceRemovedEvent(SRMListener *, SRMDevice *device)
{
    SRMLog::debug("DRM device (GPU) removed: %s.", device->name());
}

void connectorPluggedEvent(SRMListener *, SRMConnector *connector)
{
    SRMLog::debug("DRM connector plugged (%d): %s.",
                  connector->id(),
                  connector->model());
}

void connectorUnpluggedEvent(SRMListener *, SRMConnector *connector)
{
    SRMLog::debug("DRM connector unplugged (%d): %s.",
                  connector->id(),
                  connector->model());
}

int main(void)
{
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
            if (connector->connected() && connector->mmWidth() != 0)
            {
                float *phase = new float();
                if (!connector->initialize(&connectorInterface, phase))
                {
                    delete phase;
                    SRMLog::error("Failed to initialize connector %d.", connector->id());
                }

                SRMLog::log("Initialized Connector: %s.", connector->manufacturer());
            }
        }
    }

    /*
    usleep(10000000);
    printf("DIE\n");
    exit(0);
    */

    while (1)
    {
        // Poll DRM devices/connectors hotplugging events (0 disables timeout)
        if (srm->processMonitor(-1) < 0)
            break;
    }

    // Unsuscribe to DRM events
    delete deviceCreatedListener;
    delete connectorPluggedListener;

    return 0;
}
