/*
 * Proj: srm-basic example
 *
 * Auth: Eduardo Hopperdietzel
 *
 * Desc: This changes the background color each frame to all
 *       avaliable connectors until CTRL+C is pressed.
 */

#include <SRMCore.h>
#include <SRMDevice.h>
#include <SRMListener.h>
#include <SRMCrtc.h>
#include <SRMEncoder.h>
#include <SRMPlane.h>
#include <SRMConnector.h>
#include <SRMConnectorMode.h>
#include <SRMBuffer.h>

#include <SRMList.h>
#include <SRMLog.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

float color = 0.f;

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

static void initializeGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(userData);

    /* You must not do any drawing here as it won't make it to
     * the screen. */

    SRMConnectorMode *mode = srmConnectorGetCurrentMode(connector);

    glViewport(0, 
               0, 
               srmConnectorModeGetWidth(mode), 
               srmConnectorModeGetHeight(mode));

    // Schedule a repaint (this eventually calls paintGL() later, not directly)
    srmConnectorRepaint(connector);
}

static void paintGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(userData);

    glClearColor((sinf(color) + 1.f) / 2.f,
                 (sinf(color * 0.5f) + 1.f) / 2.f,
                 (sinf(color * 0.25f) + 1.f) / 2.f,
                 1.f);

    color += 0.01f;

    if (color > M_PI*4.f)
        color = 0.f;

    glClear(GL_COLOR_BUFFER_BIT);
    srmConnectorRepaint(connector);
}

static void resizeGL(SRMConnector *connector, void *userData)
{
    /* You must not do any drawing here as it won't make it to
     * the screen.
     * This is called when the connector changes its current mode,
     * set with srmConnectorSetMode() */

    // Reuse initializeGL() as it only sets the viewport
    initializeGL(connector, userData);
}

static void pageFlipped(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(connector);
    SRM_UNUSED(userData);

    /* You must not do any drawing here as it won't make it to
     * the screen.
     * This is called when the last rendered frame is now being
     * displayed on screen.
     * Google v-sync for more info. */
}

static void uninitializeGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(connector);
    SRM_UNUSED(userData);

    /* You must not do any drawing here as it won't make it to
     * the screen.
     * Here you should free any resource created on initializeGL()
     * like shaders, programs, textures, etc. */
}

static SRMConnectorInterface connectorInterface =
{
    .initializeGL = &initializeGL,
    .paintGL = &paintGL,
    .resizeGL = &resizeGL,
    .pageFlipped = &pageFlipped,
    .uninitializeGL = &uninitializeGL
};

static void connectorPluggedEventHandler(SRMListener *listener, SRMConnector *connector)
{
    SRM_UNUSED(listener);

    /* This is called when a new connector is avaliable (E.g. Plugging an HDMI display). */

    /* Got a new connector, let's render on it */
    if (!srmConnectorInitialize(connector, &connectorInterface, NULL))
        SRMError("[srm-basic] Failed to initialize connector %s.",
                 srmConnectorGetModel(connector));
}

static void connectorUnpluggedEventHandler(SRMListener *listener, SRMConnector *connector)
{
    SRM_UNUSED(listener);
    SRM_UNUSED(connector);

    /* This is called when a connector is no longer avaliable (E.g. Unplugging an HDMI display). */

    /* The connnector is automatically uninitialized after this event (if initialized)
     * so calling srmConnectorUninitialize() is a no-op. */
}

int main(void)
{
    setenv("SRM_DEBUG", "4", 1);
    setenv("SRM_EGL_DEBUG", "4", 1);

    SRMCore *core = srmCoreCreate(&srmInterface, NULL);

    if (!core)
    {
        SRMFatal("[srm-basic] Failed to initialize SRM core.");
        return 1;
    }

    // Subscribe to Evdev events
    SRMListener *connectorPluggedEventListener = srmCoreAddConnectorPluggedEventListener(core, &connectorPluggedEventHandler, NULL);
    SRMListener *connectorUnpluggedEventListener = srmCoreAddConnectorUnpluggedEventListener(core, &connectorUnpluggedEventHandler, NULL);

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
                    SRMError("[srm-basic] Failed to initialize connector %s.",
                             srmConnectorGetModel(connector));
            }
        }
    }

    while (1)
    {
        /* Evdev monitor poll DRM devices/connectors hotplugging events (-1 disables timeout).
         * To get a pollable FD use srmCoreGetMonitorFD() */

        if (srmCoreProccessMonitor(core, -1) < 0)
            break;
    }

    /* Unsubscribe to DRM events
     *
     * These listeners are automatically destroyed when calling srmCoreDestroy()
     * so there is no need to free them manually.
     * This is here just to show how to unsubscribe to events on the fly. */

    srmListenerDestroy(connectorPluggedEventListener);
    srmListenerDestroy(connectorUnpluggedEventListener);

    // Finish SRM
    srmCoreDestroy(core);

    return 0;
}
