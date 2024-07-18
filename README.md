# Simple Rendering Manager

<p align="left">
  <a href="https://github.com/CuarzoSoftware/SRM/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="SRM is released under the MIT license." />
  </a>
  <a href="https://github.com/CuarzoSoftware/SRM">
    <img src="https://img.shields.io/badge/version-0.7.0-brightgreen" alt="Current SRM version." />
  </a>
</p>

SRM is a C library that simplifies the development of Linux DRM/KMS applications.

With SRM, you can focus on the OpenGL ES 2.0 logic of your application. For each available display, you can start a rendering thread that triggers common events like **initializeGL()**, **paintGL()**, **resizeGL()**, **pageFlipped()** and **uninitializeGL()**.

It also ensures textures can be rendered across all screens, even if they are attached to different GPUs.

### Links

* [📖 C API Documentation](https://cuarzosoftware.github.io/SRM/modules.html)
* [🎓 Tutorial](https://cuarzosoftware.github.io/SRM/md_md__tutorial.html)
* [🕹️ Examples](https://cuarzosoftware.github.io/SRM/md_md__examples.html)
* [📦 Downloads](https://cuarzosoftware.github.io/SRM/md_md__downloads.html)
* [⚙️ Environment](https://cuarzosoftware.github.io/SRM/md_md__envs.html)
* [💬 Contact](https://cuarzosoftware.github.io/SRM/md_md__contact.html)

### Used By

SRM is the main graphic backend used by the [Louvre C++ Wayland Library](https://github.com/CuarzoSoftware/Louvre), as depicted in the image below.

![Louvre Example](https://lh3.googleusercontent.com/pw/AIL4fc9VCmbRMl7f4ibvQqDrWpmLkXJ9W3MHHWKKE7g5oKcYSIrOut0mQEb1sDoblm9h35zUXk5zhwOwlWnM-soCtjeznhmA7yfRNqo-5a3PdwNYapM1vn4=w2400)

### Features

* Support for multi-GPU setups
* Automatic configuration of GPUs and connectors
* Texture allocation from main memory, DMA Buffers, GBM BOs and Wayland DRM Buffers
* Multi-session capability (e.g. can use [libseat](https://github.com/kennylevinsen/seatd) to open DRM devices)
* Listener for connectors hot-plugging events
* Simple cursor planes control
* V-Sync control (support for the atomic DRM API requires Linux >= 6.8)
* Framebuffer damage (enhances performance in multi-GPU setups where DMA support is lacking)
* Access to screen framebuffers as textures
* Support for double and triple buffering
* Gamma correction

### Tested on

* Intel GPUs (i915 driver)
* NVIDIA GPUs (see [Nvidia Configuration](https://cuarzosoftware.github.io/SRM/md_md__envs.html))
* AMD GPUs (AMDGPU driver)
* Mali GPUs (Lima driver)

### Rendering Modes

In multi-GPU setups, connectors such as eDP, HDMI, DisplayPort, etc., can be attached to different GPUs. However, not all GPUs may support texture sharing, which could prevent, for example, a compositor from dragging a window across screens. Therefore, SRM assigns one of the following rendering modes to each GPU to ensure textures can always be rendered on all displays:

**SELF MODE**: If there's a single GPU or one that can handle all buffer formats from the allocator device, it directly renders into its own connectors, which is the best case.

**PRIME MODE**: When there are multiple GPUs and one cannot support all formats from the allocator, another GPU handles the rendering into a DMA-importable buffer, which is then blitted into the screen framebuffer. This approach is fast but not optimal, as only one GPU performs the "heavy" rendering.

**DUMB MODE**: In setups with multiple GPUs where one can't import buffers but supports [dumb buffers](https://manpages.debian.org/testing/libdrm-dev/drm-memory.7.en.html), another GPU handles rendering and copies the result to dumb buffers for direct scanout. This is sub-optimal as only one GPU does the rendering and involves a GPU-to-CPU copy.

**CPU MODE**: Similar to the previous case, but when dumb buffers are not supported, the rendered content is copied to main memory. Then, a texture supported by the affected GPU is updated from the pixel data and rendered into the screen framebuffer. This is the worst-case scenario as it involves CPU copying for both reading and uploading to GPUs.

Performance in the last three modes can be significantly improved by specifying the damage generated during a `paintGL()` event using `srmConnectorSetBufferDamage()`.

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
