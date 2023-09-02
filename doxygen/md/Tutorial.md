# Tutorial

In this tutorial, you will learn the basics of SRM to kickstart your journey into creating DRM/KMS applications with OpenGL ES 2.0. 
Let's begin by creating a empty project directory, a `main.c` file and `meson.build` file. In this example, we will use Meson for our build system.

### main.c

```
int main()
{
    return 0;
}
```

### meson.build

```
project('srm-example',
        'c',
        version : '0.1.0')

c = meson.get_compiler('c')

library_paths = ['/usr/lib']

glesv2_dep = c.find_library('GLESv2', dirs: library_paths, required: true)
srm_dep = c.find_library('SRM', dirs: library_paths, required: true)

sources = ['main.c']

executable(
    'srm-example',
    sources,
    dependencies: [glesv2_dep, srm_dep])
```

Now, let's configure the project by running the following commands in your project directory:

```bash
cd your_project_dir
meson setup builddir
```

You should observe output confirming that the GLESv2 and SRM libraries have been found, and a new `builddir` directory should be created. 

```bash
...

Host machine cpu family: x86_64
Host machine cpu: x86_64
Library GLESv2 found: YES
Library SRM found: YES
Build targets in project: 1

Found ninja-1.10.1 at /usr/bin/ninja
```

If this is not the case, and the libraries are not found, please double-check that you have installed the GLESv2 and SRM libraries correctly, or investigate if any environment configuration adjustments are necessary.

Now, in `main.c` let's set up an interface that allows SRM to open and close DRM devices.

```c
#include <SRM/SRMCore.h>
#include <SRM/SRMDevice.h>
#include <SRM/SRMConnector.h>
#include <SRM/SRMConnectorMode.h>
#include <SRM/SRMList.h>
#include <SRM/SRMLog.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

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
```

This interface primarily involves managing the opening and closing of DRM file descriptors. Instead of relying solely on the open() and close() functions, you might consider incorporating a library like [libseat](https://github.com/kennylevinsen/seatd) to enhance your program's compatibility with multi-seat setups, enabling seamless tty switching.

Let's proceed by creating an SRMCore instance using this interface, which will handle the setup and configuration of all devices on your behalf. In case the creation of SRMCore encounters an error, we will gracefully exit the program.

```c
// ...

int main()
{
    SRMCore *core = srmCoreCreate(&srmInterface, NULL);

    if (!core)
    {
        SRMFatal("[srm-example] Failed to create SRMCore.");
        return 0;
    }

    srmCoreDestroy(core);

    return 0;
}
```

#### Devices and Connectors

Now lets enumerate all avaliable devices (GPUs) and their respective connectors (displays).

```c
// ...

int main()
{
    SRMCore *core = srmCoreCreate(&srmInterface, NULL);

    if (!core)
    {
        SRMFatal("[srm-example] Failed to create SRMCore.");
        return 0;
    }

    // Loop each GPU (device)
    SRMListForeach (deviceIt, srmCoreGetDevices(core))
    {
        SRMDevice *device = srmListItemGetData(deviceIt);

        SRMLog("[srm-example] Device %s connectors:", srmDeviceGetName(device));

        // Loop each GPU connector (screen)
        SRMListForeach (connectorIt, srmDeviceGetConnectors(device))
        {
            SRMConnector *connector = srmListItemGetData(connectorIt);
            SRMLog("[srm-example] - Connector %d %s %s %s.",
                   srmConnectorGetID(connector),
                   srmConnectorGetName(connector),
                   srmConnectorGetModel(connector),
                   srmConnectorGetManufacturer(connector));
        }
    }

    srmCoreDestroy(core);

    return 0;
}
```

Lets compile the program by running the following:

```bash
cd builddir
meson compile
```

If there are no errors during the build process, you should find a new executable file in the `builddir` directory named `srm-example`. You can run it with the following command:

```bash
./srm-example
```

The output should display a list of devices along with their respective connector information. For example, on my machine, the output appears as follows:

```bash
[srm-example] Device /dev/dri/card0 connectors:
[srm-example] - Connector 77 eDP-0 Color LCD Apple Computer Inc.
[srm-example] - Connector 84 DisplayPort-1 Unknown Unknown.
[srm-example] - Connector 92 HDMI-A-1 Unknown Unknown.
[srm-example] - Connector 98 DisplayPort-0 Unknown Unknown.
[srm-example] - Connector 104 HDMI-A-2 Unknown Unknown.
[srm-example] - Connector 108 HDMI-A-0 Unknown Unknown.
```

Please note that in the output, you may see connectors listed as "Unknown" for model and manufacturer if no display is attached to those connectors, which is expected behavior.

#### Rendering

Now, let's explore rendering to the available connectors. To do this, we'll set up a common interface to handle OpenGL rendering events, which will be shared across all connectors.

```c
// ...

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

// ...
```

Lets see what each function does:

* **initializeGL:** This function is called once after a connector is initialized. Its purpose is to set up all the necessary OpenGL resources, such as shaders, texture loading, etc. In this specific case, it configures the viewport using the dimensions of the current connector mode. A connector can have multiple modes, each defining resolution and refresh rate. Additionally, it calls srmConnectorRepaint(), which schedules a new rendering frame.

* **resizeGL:** This function is triggered when the current connector mode changes, typically when the resolution is adjusted. Here, the main task is to update the viewport to match the new dimensions.

* **paintGL:** In this function, you should perform all the OpenGL rendering operations required for the current frame. In the provided example, the screen is cleared with a random color in each frame.

* **pageFlipped:** This function is called when the last rendered frame is being displayed on the screen.

* **uninitializeGL:** This is called just before a connector is uninitialized. Here you should free the resources created in initializeGL.


Now lets use this interface to initialize all connected connectors.

```c
// ...

int main()
{
    SRMCore *core = srmCoreCreate(&srmInterface, NULL);

    if (!core)
    {
        SRMFatal("[srm-example] Failed to create SRMCore.");
        return 0;
    }

    // Loop each GPU (device)
    SRMListForeach (deviceIt, srmCoreGetDevices(core))
    {
        SRMDevice *device = srmListItemGetData(deviceIt);

        SRMLog("[srm-example] Device %s connectors:", srmDeviceGetName(device));

        // Loop each GPU connector (screen)
        SRMListForeach (connectorIt, srmDeviceGetConnectors(device))
        {
            SRMConnector *connector = srmListItemGetData(connectorIt);
            SRMLog("[srm-example] - Connector %d %s %s %s.",
                   srmConnectorGetID(connector),
                   srmConnectorGetName(connector),
                   srmConnectorGetModel(connector),
                   srmConnectorGetManufacturer(connector));

            // Check if there is a display attached
            if (srmConnectorIsConnected(connector))
            {
                // Initialize the connector
                if (!srmConnectorInitialize(connector, &connectorInterface, NULL))
                {
                    SRMError("[srm-example] Failed to initialize connector %s",
                             srmConnectorGetName(connector));
                }
            }
        }
    }

    // Sleep 10 secs
    usleep(10000000);

    srmCoreDestroy(core);

    return 0;
}
```

Now, we're checking each connector's display attachment status using srmConnectorIsConnected() and initializing them with srmConnectorInitialize().

Additionally, note that we've included a usleep() call at the end to wait for 10 seconds. This delay is necessary because each connector performs its rendering in its own thread. Blocking the main thread ensures that it doesn't exit immediately.

Before running the program again, switch to a free terminal (tty) and launch it from there. You should observe your displays changing colors rapidly for 10 seconds.

### Hotplugging Events

We've previously covered how to check for available connectors during startup. However, what if one of those connectors becomes disconnected while the program is running, such as unplugging an HDMI display? To handle connector hotplugging events, you'll need to add the following event listeners:

```c
// ...

static void connectorPluggedEventHandler(SRMListener *listener, SRMConnector *connector)
{
    SRM_UNUSED(listener);

    /* This is called when a new connector is avaliable (E.g. Plugging an HDMI display). */

    /* Got a new connector, let's render on it */
    if (!srmConnectorInitialize(connector, &connectorInterface, NULL))
        SRMError("[srm-example] Failed to initialize connector %s",
                             srmConnectorGetName(connector));
}

static void connectorUnpluggedEventHandler(SRMListener *listener, SRMConnector *connector)
{
    SRM_UNUSED(listener);
    SRM_UNUSED(connector);

    /* This is called when a connector is no longer avaliable (E.g. Unplugging an HDMI display). */

    /* The connnector is automatically uninitialized after this event (if initialized)
     * so calling srmConnectorUninitialize() is a no-op. */
}

int main()
{
    SRMCore *core = srmCoreCreate(&srmInterface, NULL);

    if (!core)
    {
        SRMFatal("[srm-example] Failed to create SRMCore.");
        return 0;
    }

    // Subscribe to Uvdev events
    srmCoreAddConnectorPluggedEventListener(core, &connectorPluggedEventHandler, NULL);
    srmCoreAddConnectorUnpluggedEventListener(core, &connectorUnpluggedEventHandler, NULL);

    // Loop each GPU (device)
    SRMListForeach (deviceIt, srmCoreGetDevices(core))
    {
        SRMDevice *device = srmListItemGetData(deviceIt);

        SRMLog("[srm-example] Device %s connectors:", srmDeviceGetName(device));

        // Loop each GPU connector (screen)
        SRMListForeach (connectorIt, srmDeviceGetConnectors(device))
        {
            SRMConnector *connector = srmListItemGetData(connectorIt);
            SRMLog("[srm-example] - Connector %d %s %s %s.",
                   srmConnectorGetID(connector),
                   srmConnectorGetName(connector),
                   srmConnectorGetModel(connector),
                   srmConnectorGetManufacturer(connector));

            if (srmConnectorIsConnected(connector))
            {
                if (!srmConnectorInitialize(connector, &connectorInterface, NULL))
                {
                    SRMError("[srm-example] Failed to initialize connector %s",
                             srmConnectorGetName(connector));
                }
            }
        }
    }

    while (1)
    {
        /* Udev monitor poll DRM devices/connectors hotplugging events (-1 disables timeout).
         * To get a pollable FD use srmCoreGetMonitorFD() */

        if (srmCoreProccessMonitor(core, -1) < 0)
            break;
    }

    srmCoreDestroy(core);

    return 0;
}
```

Now, each time a new connector becomes available, `connectorPluggedEventHandler()` will be invoked, allowing us to initialize the new connector. Similarly, we can detect when a connector is disconnected using `connectorUnpluggedEventHandler()`. If an initialized connector gets disconnected, it is automatically uninitialized, triggering the `uninitializeGL()` function.

One notable change is the replacement of the `usleep()` function with an infinite `while` loop. Within this loop, we poll a `uvdev` monitor file descriptor using the `srmCoreProcessMonitor()` function. This change is necessary to allow SRM to invoke hotplugging events.

To test these changes, recompile the program and try connecting and disconnecting an external display on the fly. You should observe that it is automatically initialized and uninitialized each time, reflecting the hotplugging events.

### Buffers

SRM enables the creation of buffers (OpenGL textures) from various sources, including DMA planes, GBM buffers, Wayland DRM buffers, and main memory. These buffers can be used for rendering on all connectors, even if they belong to different devices. 

Let's see how to create a buffer from main memory:

```c
#include <SRM/SRMBuffer.h>

// ...

// A 128 x 256 RGBA image in main memory
UInt8 pixelsSource[128 * 256 * 4];

// Pass NULL as the allocator device to share the buffer across all devices
SRMBuffer *buffer = srmBufferCreateFromCPU(
    core, // SRM core
    NULL, // allocator device
    128, // src width
    256, // src height
    128 * 4, // src stride
    pixelsSource, 
    DRM_FORMAT_ARGB8888);

if (!buffer)
{
    SRMError("Failed to create a buffer from main memory.");
    exit(1);
}

// ...

// Use the buffer in a connector rendering thread (paintGL)

// First, get the device this connector belongs to
SRMDevice *connectorDevice = srmConnectorGetDevice(connector);

// Then, get the device responsible for rendering for the connector device (usually the same device)
SRMDevice *connectorRendererDevice = srmDeviceGetRendererDevice(connectorDevice);

// Retrieve the OpenGL texture ID
GLuint textureId = srmBufferGetTextureID(connectorRendererDevice, buffer);

if (textureId == 0)
    SRMError("Failed to get the GL texture ID from SRMBuffer.");

// ...
```

It's important to note that all buffers are shared across all devices, except for those created from GBM buffers or Wayland DRM buffers, which may not always be supported by all devices.