# Simple Rendering Manager

<p align="left">
  <a href="https://github.com/CuarzoSoftware/SRM/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/license-GPL_v3.0-blue.svg" alt="SRM is released under the GPL v3.0 license." />
  </a>
  <a href="https://github.com/CuarzoSoftware/SRM">
    <img src="https://img.shields.io/badge/version-0.3.0-brightgreen" alt="Current SRM version." />
  </a>
</p>

SRM is a C library that simplifies the development of Linux DRM/KMS applications.

With SRM, you can focus on the OpenGL ES 2.0 logic of your application. For each available display, you can start a rendering thread that triggers common events like **initializeGL()**, **paintGL()**, **resizeGL()**, **pageFlipped()** and **uninitializeGL()**.

SRM allows you to use multiple GPUs simultaneously and automatically finds the most efficient configuration. It also offers functions for creating OpenGL textures, which are automatically shared among GPUs.

### Links

* [📖 C API Documentation](https://cuarzosoftware.github.io/SRM/modules.html)
* [🎓 Tutorial](https://cuarzosoftware.github.io/SRM/md_md__tutorial.html)
* [🕹️ Examples](https://cuarzosoftware.github.io/SRM/md_md__examples.html)
* [📦 Downloads](https://cuarzosoftware.github.io/SRM/md_md__downloads.html)
* [💬 Contact](https://cuarzosoftware.github.io/SRM/md_md__contact.html)

### Used By

SRM is the main graphic backend used by the [Louvre C++ Wayland Library](https://github.com/CuarzoSoftware/Louvre), as depicted in the image below.

![Louvre Example](https://lh3.googleusercontent.com/pw/AIL4fc9VCmbRMl7f4ibvQqDrWpmLkXJ9W3MHHWKKE7g5oKcYSIrOut0mQEb1sDoblm9h35zUXk5zhwOwlWnM-soCtjeznhmA7yfRNqo-5a3PdwNYapM1vn4=w2400)

### Features

* Multiple GPUs support
* Automatic optimal GPUs/connectors configuration
* Automatic texture sharing between GPUs
* Texture allocation from CPU buffers, DMA buffers, GBM BOs, Flink Handles, Wayland DRM buffers.
* Multi seat support ([libseat](https://github.com/kennylevinsen/seatd) can be used to open DRM devices for example)
* GPU hot-plugging event listener
* Connectors hot-plugging event listener
* Hardware cursor compositing
* V-Sync
* Frame buffer damage (improves performance when DMA is not supported)
* Access to renderbuffers as textures.

### Tested on

* Intel GPUs (i915 driver)
* NVIDIA GPUs (Nouveau and propietary drivers)
* Mali GPUs (Lima driver)

### Basic Example

```c
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
#include <unistd.h>

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
     * so calling srmConnectorUninitialize() here is not required. */
}

int main(void)
{
    SRMCore *core = srmCoreCreate(&srmInterface, NULL);

    if (!core)
    {
        SRMFatal("[srm-basic] Failed to initialize SRM core.");
        return 1;
    }

    // Subscribe to Udev events
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
        /* Udev monitor poll DRM devices/connectors hotplugging events (-1 disables timeout).
         * To get a pollable FD use srmCoreGetMonitorFD() */

        if (srmCoreProcessMonitor(core, -1) < 0)
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
```
