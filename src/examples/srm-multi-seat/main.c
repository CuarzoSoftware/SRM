/*
 * Project: srm-multi-seat Example
 *
 * Author: Eduardo Hopperdietzel
 *
 * Description: This example changes the background color of all available
 *              connectors when the hardware cursor changes position until
 *              the ESC key is pressed. It also demonstrates how to enable
 *              multi-seat support and integrate input events with Libinput.
 *              To switch to different ttys, use CTRL-ALT-F[1, 2, ..., 10],
 *              and to move the hardware cursor use any pointing device like
 *              a mouse or touchpad.
 */
#include <SRMCore.h>
#include <SRMDevice.h>
#include <SRMConnector.h>
#include <SRMConnectorMode.h>
#include <SRMListener.h>

#include <SRMList.h>
#include <SRMLog.h>

#include <GLES2/gl2.h>
#include <libseat.h>
#include <libinput.h>
#include <libudev.h>
#include <linux/input-event-codes.h>

#include <math.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/poll.h>

static SRMCore *core = NULL;
struct libseat *seat = NULL;
struct libinput *input = NULL;
struct udev *udv = NULL;

// [0] SRM Monitor [1] Libseat [2] Libinput
static struct pollfd fds[3];

// Device ID / FD pair
typedef struct SeatDevice
{
    Int32 id;
    Int32 fd;
} SeatDevice;
static SRMList *seatDevices = NULL;

// Pressed keys
static SRMList *pressedKeys = NULL;

// Hardware cursor pixels
UInt8 cursorPixels[64 * 64 * 4];

// Hardare cursor pos relative to its connector
double cursorX = 0, cursorY = 0;

// Var to update the background color
static float color = 0.f;

/* Open an EVDEV or DRM device */
static int openRestricted(const char *path, int flags, void *userData)
{
    SRM_UNUSED(userData);
    SRM_UNUSED(flags);

    SeatDevice *device = malloc(sizeof(SeatDevice));
    device->id = libseat_open_device(seat, path, &device->fd);
    srmListAppendData(seatDevices, device);

    return device->fd;
}

/* Close an EVDEV or DRM device */
static void closeRestricted(int fd, void *userData)
{
    SRM_UNUSED(userData);

    // Find the device ID
    SeatDevice *device = NULL;

    SRMListForeach(devIt, seatDevices)
    {
        SeatDevice *dev = srmListItemGetData(devIt);

        if (dev->fd == fd)
        {
            device = dev;
            srmListRemoveItem(seatDevices, devIt);
            break;
        }
    }

    if (!device)
        return;

    libseat_close_device(seat, device->id);
    free(device);
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

    // Setup hardware cursor
    srmConnectorSetCursor(connector, cursorPixels);
    srmConnectorSetCursorPos(connector, 0, 0);

    // Schedule a repaint (this eventually calls paintGL() later, not directly)
    srmConnectorRepaint(connector);
}

static void paintGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(userData);
    SRM_UNUSED(connector);

    glClearColor((sinf(color) + 1.f) / 2.f,
                 (sinf(color * 0.5f) + 1.f) / 2.f,
                 (sinf(color * 0.25f) + 1.f) / 2.f,
                 1.f);

    color += 0.01f;

    if (color > M_PI*4.f)
        color = 0.f;

    glClear(GL_COLOR_BUFFER_BIT);
}

static void resizeGL(SRMConnector *connector, void *userData)
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

static void pageFlipped(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(connector);
    SRM_UNUSED(userData);
}

static void uninitializeGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(connector);
    SRM_UNUSED(userData);
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
        SRMError("[srm-multi-seat] Failed to initialize connector %s.",
                 srmConnectorGetModel(connector));
}

/* This is called when the TTY session is restored.
 * So we can now resume the previusly paused connectors rendering threads
 * and Libinput as well. */
static void enableSeat(struct libseat *seat, void *userdata)
{
    SRM_UNUSED(seat);
    SRM_UNUSED(userdata);

    SRMListForeach (deviceIt, srmCoreGetDevices(core))
    {
        SRMDevice *device = srmListItemGetData(deviceIt);

        SRMListForeach (connectorIt, srmDeviceGetConnectors(device))
        {
            SRMConnector *connector = srmListItemGetData(connectorIt);
            srmConnectorResume(connector);

            // You may also need to restore the hardware cursor
            // if it was used in other sessions
            srmConnectorSetCursor(connector, NULL);
            srmConnectorSetCursor(connector, cursorPixels);
            srmConnectorSetCursorPos(connector, cursorX + 1, cursorY);
        }
    }

    libinput_resume(input);
}

/* This is called when switching to another TTY session.
 * So we pause all connectors rendering threads, and suspend
 * Libinput. */
static void disableSeat(struct libseat *seat, void *userdata)
{
    SRM_UNUSED(userdata);

    SRMListForeach (deviceIt, srmCoreGetDevices(core))
    {
        SRMDevice *device = srmListItemGetData(deviceIt);

        SRMListForeach (connectorIt, srmDeviceGetConnectors(device))
        {
            SRMConnector *connector = srmListItemGetData(connectorIt);
            srmConnectorPause(connector);
        }
    }

    libinput_suspend(input);
    libseat_disable_seat(seat);
}

static struct libseat_seat_listener libseatInterface =
{
    .enable_seat = &enableSeat,
    .disable_seat = &disableSeat
};

static UInt8 isKeyPressed(UInt32 keyCode)
{
    SRMListForeach(keyIt, pressedKeys)
    {
        UInt32 *key = srmListItemGetData(keyIt);

        if (*key == keyCode)
            return 1;
    }

    return 0;
}

static void handleInputEvents()
{
    int ret = libinput_dispatch(input);

    if (ret != 0)
    {
        SRMError("[Libinput Backend] Failed to dispatch libinput %s.", strerror(-ret));
        return;
    }

    struct libinput_event *ev;

    while ((ev = libinput_get_event(input)) != NULL)
    {
        switch (libinput_event_get_type(ev))
        {
        case LIBINPUT_EVENT_POINTER_MOTION:
        {
            struct libinput_event_pointer *pointerEvent = libinput_event_get_pointer_event(ev);

            double dx = libinput_event_pointer_get_dx(pointerEvent);
            double dy = libinput_event_pointer_get_dy(pointerEvent);

            cursorX += dx;
            cursorY += dy;

            // Update hardware cursor position in all connectors
            SRMListForeach (deviceIt, srmCoreGetDevices(core))
            {
                SRMDevice *device = srmListItemGetData(deviceIt);

                SRMListForeach (connectorIt, srmDeviceGetConnectors(device))
                {
                    SRMConnector *connector = srmListItemGetData(connectorIt);
                    srmConnectorSetCursorPos(connector, cursorX, cursorY);
                    srmConnectorRepaint(connector);
                }
            }

        }break;
        case LIBINPUT_EVENT_KEYBOARD_KEY:
        {
            struct libinput_event_keyboard *keyEvent = libinput_event_get_keyboard_event(ev);

            // Add keyCode to the list of pressed keys
            if (libinput_event_keyboard_get_key_state(keyEvent) == LIBINPUT_KEY_STATE_PRESSED)
            {
                UInt32 *keyCode = malloc(sizeof(UInt32));
                *keyCode = libinput_event_keyboard_get_key(keyEvent);
                srmListAppendData(pressedKeys, keyCode);
            }

            // Remove keyCode from list of pressed keys
            else
            {
                UInt32 keyCode = libinput_event_keyboard_get_key(keyEvent);

                SRMListForeach(keyIt, pressedKeys)
                {
                    UInt32 *key = srmListItemGetData(keyIt);

                    if (*key == keyCode)
                    {
                        srmListRemoveItem(pressedKeys, keyIt);
                        free(key);
                        break;
                    }
                }
            }

            // Switch TTY (Function keys (F1 to F10) range from 59 to 68)
            if (isKeyPressed(KEY_LEFTCTRL) && isKeyPressed(KEY_LEFTALT))
            {
                UInt32 keyCode = libinput_event_keyboard_get_key(keyEvent);

                if (keyCode >= KEY_F1 && keyCode <= KEY_F10)
                {
                    libseat_switch_session(seat, keyCode - 58);
                    libinput_event_destroy(ev);
                    libinput_dispatch(input);
                    return;
                }
            }

        }break;
        default:
            break;
        }

        libinput_event_destroy(ev);
        libinput_dispatch(input);
    }
}

int main(void)
{
    /* 1. Setup Libseat */

    seat = libseat_open_seat(&libseatInterface, NULL);

    if (!seat)
    {
        SRMFatal("[srm-multi-seat] Failed to open seat.");
        return 1;
    }

    seatDevices = srmListCreate();

    /* 2. Setup SRM */

    core = srmCoreCreate(&srmInterface, NULL);

    if (!core)
    {
        SRMFatal("[srm-multi-seat] Failed to initialize SRM core.");
        return 1;
    }

    // Subscribe to Udev events
    srmCoreAddConnectorPluggedEventListener(core, &connectorPluggedEventHandler, NULL);

    // Create a black triangle as cursor
    memset(cursorPixels, 0, sizeof(cursorPixels));

    for (Int32 x = 0; x < 64; x++)
    {
        for (Int32 y = 0; y < 64; y++)
        {
            if (x < 24 && y < 24 && x <= 23 - y)
            {
                Int32 i = (x + y * 64) * 4;

                // Set alpha
                cursorPixels[i + 3] = 255;
            }
        }
    }

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
                    SRMError("[srm-multi-seat] Failed to initialize connector %s.",
                             srmConnectorGetModel(connector));
            }
        }
    }

    /* 3. Setup Libinput */
    udv = udev_new();

    // Use the same SRM interface (is equal to struct libinput_interface)
    input = libinput_udev_create_context((struct libinput_interface*)&srmInterface, NULL, udv);

    if (!input)
    {
        SRMFatal("[srm-multi-seat] Failed to initialize Libinput.");
        return 1;
    }

    pressedKeys = srmListCreate();

    libinput_udev_assign_seat(input, libseat_seat_name(seat));
    libinput_dispatch(input);

    /* 4. Setup POLL and main loop */

    // This one is for listening to connectors hotplugging events
    fds[0].events = POLLIN;
    fds[0].fd = srmCoreGetMonitorFD(core);
    fds[0].revents = 0;

    // This one is for listening to TTY switching events (enable / disable seat)
    fds[1].events = POLLIN;
    fds[1].fd = libseat_get_fd(seat);
    fds[1].revents = 0;

    // This one is for listening to input events
    fds[2].events = POLLIN;
    fds[2].fd = libinput_get_fd(input);
    fds[2].revents = 0;

    while (1)
    {
        poll(fds, 3, -1);

        // SRM
        if (fds[0].revents & POLLIN)
            srmCoreProccessMonitor(core, 0);

        // SEAT
        if (fds[1].revents & POLLIN)
            libseat_dispatch(seat, 0);

        // INPUT
        if (fds[2].revents & POLLIN)
            handleInputEvents();

        // EXIT
        if (isKeyPressed(KEY_ESC))
            break;
    }

    // Destroy pressed keys
    while (!srmListIsEmpty(pressedKeys))
    {
        SRMListItem *it = srmListGetBack(pressedKeys);
        UInt32 *keyCode = srmListItemGetData(it);
        srmListRemoveItem(pressedKeys, it);
        free(keyCode);
    }
    srmListDestroy(pressedKeys);

    // Finish Libinput
    libinput_unref(input);
    udev_unref(udv);

    // Finish SRM
    srmCoreDestroy(core);
    srmListDestroy(seatDevices);

    // Finish Libseat
    libseat_close_seat(seat);

    return 0;
}
