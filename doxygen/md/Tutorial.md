# 🎓 Tutorial {#tutorial_page}

In this tutorial, you will learn the basics of SRM to kickstart your journey into creating DRM/KMS applications with OpenGL ES 2.0. 

Let's begin by creating an empty project directory, with a `main.c` and `meson.build` file inside. In this example, we will use Meson as our build system.

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

pkg = import('pkgconfig')
glesv2_dep = dependency('glesv2')
srm_dep = dependency('SRM')
m_dep = c.find_library('m')
   
sources = ['main.c']
 
executable(
    'srm-example',
    sources,
    dependencies: [glesv2_dep, srm_dep, m_dep])
```

Now, let's configure the project by running the following commands in your project directory:

```bash
cd your_project_dir
meson setup builddir
```

You should observe output confirming that the GLESv2, SRM and Math libraries have been found, and a new `builddir` directory should be created. 

```bash
...

Found pkg-config: /usr/bin/pkg-config (0.29.2)
Run-time dependency glesv2 found: YES 3.2
Run-time dependency srm found: YES 0.3.2
Library m found: YES
Build targets in project: 1
```

If this is not the case, and the libraries are not found, please double-check that you have installed the GLESv2 and SRM libraries correctly, or investigate if any environment configuration adjustments are necessary.

Please refer to the [Downloads](md_md__downloads.html) section for detailed installation instructions for SRM.

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
#include <math.h>

static float color = 0.f;

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

This interface handles the management of DRM file descriptors during SRMCore's device scanning process and when you call srmCoreDestroy(). 

Instead of relying solely on the `open()` and `close()` functions, you might consider incorporating a library like [libseat](https://github.com/kennylevinsen/seatd) to enhance your program's compatibility with multi-session setups, enabling seamless TTY switching (like in the [srm-multi-session](https://github.com/CuarzoSoftware/SRM/tree/main/src/examples/srm-multi-session) example).

Let's proceed by creating an `SRMCore` instance using this interface. If any errors arise during the `SRMCore` creation process, we will ensure a graceful program exit.

```c
// ...

int main()
{
    SRMCore *core = srmCoreCreate(&srmInterface, NULL);

    if (!core)
    {
        SRMFatal("[srm-example] Failed to create SRMCore.");
        return 1;
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
Here, we are simply iterating over each `SRMDevice` (GPU/DRM device) and its associated SRMConnectors (screens/displays), printing the DRM id, name, model, and manufacturer of each. Afterward, we conclude the program.

Lets compile the program by running:

```bash
cd builddir
meson compile
```

If there are no errors during the build process, you should find a new executable file in the `builddir` directory named `srm-example`. You can run it with the following command:

```bash
./srm-example
```

The output should display one or more devices along with their respective connectors information. For example, on my machine, which has a single GPU, the output appears as follows:

```bash
[srm-example] Device /dev/dri/card0 connectors:
[srm-example] - Connector 77 eDP-0 Color LCD Apple Computer Inc.
[srm-example] - Connector 84 DisplayPort-1 Unknown Unknown.
[srm-example] - Connector 92 HDMI-A-1 Unknown Unknown.
[srm-example] - Connector 98 DisplayPort-0 Unknown Unknown.
[srm-example] - Connector 104 HDMI-A-2 Unknown Unknown.
[srm-example] - Connector 108 HDMI-A-0 Unknown Unknown.
```

Please note that in the output, connectors may appear as "Unknown" for model and manufacturer if no display is attached to those connectors. This is the expected behavior.

In my case, you can observe that there is only one connected, which corresponds to my laptop screen (eDP-0). You can check the connectivity status of any connector with the srmConnectorIsConnected() function, which we will demonstrate in the upcoming sections.

#### Rendering

Now, let's delve into the process of rendering to the available connectors. Our approach involves setting up a unified interface for managing OpenGL rendering events, which will be shared across all connectors. While it's possible to employ distinct interfaces for each connector, for the sake of simplicity, we'll use a single one here.

It's of utmost importance to underscore that these events are initiated by SRM itself and should not be manually triggered by you. Additionally, it's essential to recognize that all these events are executed within the rendering thread of each connector, operating independently from the main thread.

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

Lets see what each event does:

* **initializeGL:** This event is called once after a connector is initialized with srmConnectorInitialize(). Here you should set up all your necessary OpenGL resources, such as shaders, texture loading, etc. In this specific case, it configures the viewport using the dimensions of the current connector mode (`SRMConnectorMode`). A connector can have multiple modes, each defining resolution and refresh rate. Additionally, it calls srmConnectorRepaint(), which schedules a new rendering frame (`paintGL()` call) asynchronously.

* **resizeGL:** This event is triggered when the current connector mode changes (set with srmConnectorSetMode()). Here, the main task is to update the viewport to match the new dimensions.

* **paintGL:** Inside this event handler, you should perform all the OpenGL rendering operations required for the current frame. In the provided example, the screen is simply cleared with a random color and a new frame is scheduled with srmConnectorRepaint().

* **pageFlipped:** This event is triggered when the last rendered frame (in `paintGL()`) is now being displayed on the screen (check [Multiple Buffering](https://en.wikipedia.org/wiki/Multiple_buffering)).

* **uninitializeGL:** This event is triggered just before the connector is uninitialized. Here you should free the resources created in `initializeGL()`.

Important Note: It is imperative that you avoid initializing, uninitializing, or changing a connector's mode within its rendering thread, that is, from any of the event handlers. Doing so could lead to a deadlock or even cause your program to crash. Please be aware that this behavior is slated for correction in the upcoming SRM release.

Now lets use this interface to initialize all connected connectors.

```c
// ...

int main()
{
    SRMCore *core = srmCoreCreate(&srmInterface, NULL);

    if (!core)
    {
        SRMFatal("[srm-example] Failed to create SRMCore.");
        return 1;
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

Additionally, note that we've included a `usleep()` call at the end to wait for 10 seconds. This delay is necessary because, as said before, each connector performs its rendering in its own thread. Blocking the main thread ensures that the program doesn't exit immediately.

Re-compile with `meson compile` and before running the program, switch to a free virtual terminal (TTY) by pressing `CTRL + ALT + F[1, 2, 3 ..., 10]` or with the `chvt N` command and launch it from there. You should observe your displays changing colors rapidly for 10 seconds.

If you encounter issues, please attempt to run the program with superuser privileges or by adding your user to the video group. This may resolve any potential permission-related problems.

Additionally, you have the option to set the **SRM_DEBUG** environment variable to 3 in order to enable fatal, error and warning logging messages.

### Hotplugging Events

Thus far, we've discussed the process of identifying available connectors and initializing them at program startup. However, a critical consideration is what happens if one of these connectors becomes disconnected while the program is running, such as unplugging an HDMI display.

In such scenarios, the connectors are programmed to undergo automatic uninitialization when they become disconnected, triggering their corresponding `uninitializeGL()` event. However, you do have the flexibility to include listeners to detect and respond to connectors plugging and unplugging events, as exemplified below:

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
     * so calling srmConnectorUninitialize() is not necessary. */
}

int main()
{
    SRMCore *core = srmCoreCreate(&srmInterface, NULL);

    if (!core)
    {
        SRMFatal("[srm-example] Failed to create SRMCore.");
        return 1;
    }

    // Subscribe to udev events
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

        if (srmCoreProcessMonitor(core, -1) < 0)
            break;
    }

    srmCoreDestroy(core);

    return 0;
}
```

Now, each time a new connector becomes available, `connectorPluggedEventHandler()` will be invoked, allowing us to initialize the new connector. Similarly, we can detect when a connector is disconnected using `connectorUnpluggedEventHandler()`. If an initialized connector gets disconnected, it is automatically uninitialized, triggering the `uninitializeGL()` function.

One notable change is the replacement of the `usleep()` function with an infinite `while` loop. Within this loop, we poll a `udev` monitor file descriptor using the `srmCoreProcessMonitor()` function. This change is necessary to allow SRM to check and invoke the hotplugging events.

To test these changes, recompile the program and try connecting and disconnecting an external display on the fly. You should observe that it is automatically initialized and uninitialized each time, reflecting the hotplugging events.

### Buffers

SRM lets you create buffers (OpenGL textures) from various sources, including DMA planes, GBM buffer objects, Wayland DRM buffers, and main memory buffers. These buffers can be used for rendering on all connectors, even if they belong to different devices. 

Let's see how to create a buffer from main memory:

```c
#include <SRM/SRMBuffer.h>

// ...

// 128 x 256 ARGB8 image in main memory
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

// Use the buffer in a connector rendering thread (paintGL() event)

// Retrieve the OpenGL texture ID
GLuint textureId = srmBufferGetTextureID(
    srmConnectorGetRendererDevice(connector), 
    buffer);

if (textureId == 0)
    SRMError("Failed to get the GL texture ID from SRMBuffer.");

GLenum textureTarget = srmBufferGetTextureTarget(buffer);
glBindTexture(textureTarget, textureId);

// ... glDrawArrays() 
```

It's essential to acknowledge that all buffers are shared across all devices, with the exception of those created from GBM buffers or Wayland DRM buffers, which may not always be supported by all devices.

Furthermore, you have the option to read from and write to these buffers, and they are automatically synchronized across all devices. For more in-depth information, please refer to the [SRMBuffer](https://cuarzosoftware.github.io/SRM/group___s_r_m_buffer.html) documentation.
