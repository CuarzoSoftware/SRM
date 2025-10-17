/*
 * Project: srm-direct-scanout Example
 *
 * Author: Eduardo Hopperdietzel
 *
 * Description: This example alternates between using a custom scanout buffer (black and white stripe pattern)
 *              and rendering a solid color every second for each initialized connector.
 *              Solid colors are displayed exclusively on failure.
 */

#include <SRMBuffer.h>
#include <SRMCore.h>
#include <SRMDevice.h>
#include <SRMConnector.h>
#include <SRMConnectorMode.h>
#include <SRMListener.h>

#include <SRMList.h>
#include <SRMLog.h>

#include <GLES2/gl2.h>

#include <math.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

float color = 0.f;
static UInt32 secs = 0;

/* Opens a DRM device */
static int openRestricted(const char *path, int flags, void *userData)
{
    SRM_UNUSED(userData);

    // Here something like libseat could be used instead
    return open(path, flags);
}

/* Closes a DRM device */
static void closeRestricted(int fd, void *userData)
{
    SRM_UNUSED(userData);
    close(fd);
}

static SRMInterface srmInterface =
{
    .openRestricted = &openRestricted,
    .closeRestricted = &closeRestricted
};

static void createScanoutBuffer(SRMConnector *connector)
{
    SRMConnectorMode *mode = srmConnectorGetCurrentMode(connector);
    UInt32 width = srmConnectorModeGetWidth(mode);
    UInt32 height = srmConnectorModeGetHeight(mode);
    UInt32 stride = width * 4;
    UInt32 format = DRM_FORMAT_XRGB8888;
    UInt8 *pixels = malloc(stride * height);
    UInt32 lineSize = height / 25;
    UInt8 blackWhiteToggle = 0;

    UInt8 *pixel = pixels;

    for (UInt32 y = 0; y < height; y++)
    {
        if (y % lineSize == 0)
            blackWhiteToggle = 1 - blackWhiteToggle;

        for (UInt32 x = 0; x < width; x++)
        {
            pixel[0] = pixel[1] = pixel[2] = blackWhiteToggle * 255;
            pixel+=4;
        }
    }

    SRMDevice *device = srmConnectorGetRendererDevice(connector);
    SRMCore *core = srmDeviceGetCore(device);
    SRMBuffer *scanoutBuffer = srmBufferCreateFromCPU(
        core, device, width, height, stride, pixels, format);
    srmConnectorSetUserData(connector, scanoutBuffer);
    free(pixels);
}

static void initializeGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(userData);

    SRMConnectorMode *mode = srmConnectorGetCurrentMode(connector);

    glViewport(0, 
               0, 
               srmConnectorModeGetWidth(mode), 
               srmConnectorModeGetHeight(mode));

    createScanoutBuffer(connector);
    srmConnectorRepaint(connector);
}

static void paintGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(userData);

    SRMBuffer *scanoutBuffer = srmConnectorGetUserData(connector);

    color += 0.05f;

    if (color > M_PI*4.f)
        color = 0.f;

    if (secs % 2 == 0 && scanoutBuffer && srmConnectorSetCustomScanoutBuffer(connector, scanoutBuffer))
    {
        srmConnectorRepaint(connector);
        return;
    }

    glClearColor((sinf(color) + 1.f) / 2.f,
                 (sinf(color * 0.5f) + 1.f) / 2.f,
                 (sinf(color * 0.25f) + 1.f) / 2.f,
                 1.f);

    glClear(GL_COLOR_BUFFER_BIT);
    srmConnectorRepaint(connector);
}

static void noop(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(connector);
    SRM_UNUSED(userData);
}

static void uninitializeGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(userData);

    SRMBuffer *scanoutBuffer = srmConnectorGetUserData(connector);

    if (scanoutBuffer)
    {
        srmBufferDestroy(scanoutBuffer);
        srmConnectorSetUserData(connector, NULL);
    }
}

static SRMConnectorInterface connectorInterface =
{
    .initializeGL = &initializeGL,
    .paintGL = &paintGL,
    .resizeGL = &noop,
    .pageFlipped = &noop,
    .uninitializeGL = &uninitializeGL
};

static void connectorPluggedEventHandler(SRMListener *listener, SRMConnector *connector)
{
    SRM_UNUSED(listener);

    /* This is called when a new connector is avaliable (E.g. Plugging an HDMI display). */

    /* Got a new connector, let's render on it */
    if (!srmConnectorInitialize(connector, &connectorInterface, NULL))
        SRMError("[srm-direct-scanout] Failed to initialize connector %s.",
                 srmConnectorGetModel(connector));
}

static void connectorUnpluggedEventHandler(SRMListener *listener, SRMConnector *connector)
{
    SRM_UNUSED(listener);
    SRM_UNUSED(connector);

    /* This is called when a connector is no longer avaliable (E.g. Unplugging an HDMI display). */

    /* The connnector is automatically uninitialized after this event (if initialized)
     * so calling srmConnectorUninitialize() here is not required. */
}

int main(void)
{
    SRMCore *core = srmCoreCreate(&srmInterface, NULL);

    if (!core)
    {
        SRMFatal("[srm-direct-scanout] Failed to initialize SRM core.");
        return 1;
    }

    // Subscribe to Udev events
    srmCoreAddConnectorPluggedEventListener(core, &connectorPluggedEventHandler, NULL);
    srmCoreAddConnectorUnpluggedEventListener(core, &connectorUnpluggedEventHandler, NULL);

    // Find and initialize avaliable connectors

    // Loop each GPU (device)
    SRMListForeach (deviceIt, srmCoreGetDevices(core))
    {
        SRMDevice *device = srmListItemGetData(deviceIt);

        // Loop each GPU connector (screen)
        SRMListForeach (connectorIt, srmDeviceGetConnectors(device))
        {
            SRMConnector *connector = srmListItemGetData(connectorIt);

            if (srmConnectorIsConnected(connector))
            {
                if (!srmConnectorInitialize(connector, &connectorInterface, NULL))
                    SRMError("[srm-direct-scanout] Failed to initialize connector %s.",
                             srmConnectorGetModel(connector));
            }
        }
    }

    while (1)
    {
        /* Udev monitor poll DRM devices/connectors hotplugging events (-1 disables timeout).
         * To get a pollable FD use srmCoreGetMonitorFD() */

        if (srmCoreProcessMonitor(core, 1000) < 0)
            break;

        secs++;

        if (secs > 20)
        {
            srmCoreDestroy(core);
            break;
        }
    }

    return 0;
}
