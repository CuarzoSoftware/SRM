/*
 * Proj: srm-all-connectors example
 *
 * Auth: Eduardo Hopperdietzel
 *
 * Desc: This example displays a white hardware cursor,
 *       a moving white line and a background texture to all
 *       avaliable connectors at a time until CTRL+C is pressed.
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

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

/* Hardware cursor pixels */
static UInt8 cursorPixels[64*64*4];

/* Background texture pixels */
static UInt8 bufferPixels[] =
{
    255, 0, 0, 255,     // Red
    0, 255, 0, 255,     // Green
    0, 0, 255, 255,     // Blue
    255, 255, 0, 255,   // Yellow
    255, 0, 255, 255,   // Pink
    0, 255, 255, 255,   // Cyan
};

/* Background texture shared among all GPUs */
static SRMBuffer *buffer;

/* Square vertices (left for vertex shader, right for fragment shader) */
static GLfloat square[16] =
{  //  VERTEX       FRAGMENT
    -1.0f, -1.0f,   0.f, 1.f, // TL
    -1.0f,  1.0f,   0.f, 0.f, // BL
     1.0f,  1.0f,   1.f, 0.f, // BR
     1.0f, -1.0f,   1.f, 1.f  // TR
};

static const char *vertexShaderSource = "attribute vec4 position; varying vec2 v_texcoord; void main() { gl_Position = vec4(position.xy, 0.0, 1.0); v_texcoord = position.zw; }";
static const char *fragmentShaderSource = "precision mediump float; uniform sampler2D tex; varying vec2 v_texcoord; void main() { gl_FragColor = texture2D(tex, v_texcoord); }";

struct ConnectorUserData
{
    SRMDevice *rendererDevice;
    GLuint vertexShader;
    GLuint fragmentShader;
    GLuint program;

    // Used to animate the cursor and white line
    float phase;

    // Current screen mode dimensions
    UInt32 w, h;

    // Count FPS
    UInt64 framesCount, msStart, sec;
};


UInt64 getMilliseconds()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((UInt64)tv.tv_sec * 1000) + ((UInt64)tv.tv_usec / 1000);
}

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

static void drawColorSquares(UInt32 w, UInt32 h)
{
    glViewport(0, 0, w, h);
    glScissor(0, 0, w, h);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

static void setupShaders(SRMConnector *connector, void *userData)
{
    struct ConnectorUserData *data = userData;

    data->vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(data->vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(data->vertexShader);

    // Check for vertex shader compilation errors
    GLint success = 0;
    GLchar infoLog[512];
    glGetShaderiv(data->vertexShader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        glGetShaderInfoLog(data->vertexShader, 512, NULL, infoLog);
        SRMError("Vertex shader compilation error: %s.", infoLog);
    }

    data->fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(data->fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(data->fragmentShader);

    success = 0;
    glGetShaderiv(data->fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(data->fragmentShader, 512, NULL, infoLog);
        SRMError("Fragment shader compilation error: %s.", infoLog);
    }

    data->program = glCreateProgram();
    glAttachShader(data->program, data->vertexShader);
    glAttachShader(data->program, data->fragmentShader);
    glLinkProgram(data->program);
    glUseProgram(data->program);

    // Bind vPosition to attribute 0
    glBindAttribLocation(data->program, 0, "position");

    // Load the vertex data
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, square);

    // Enables the vertex array
    glEnableVertexAttribArray(0);

    /* Calling srmBufferGetTextureID() returns a GL texture ID for a specific device.
     * In this case, we want one for the device that do the rendering for this connector
     * (connector->device->rendererDevice) */

}

static void initializeGL(SRMConnector *connector, void *userData)
{
    struct ConnectorUserData *data = userData;

    SRMConnectorMode *mode = srmConnectorGetCurrentMode(connector);
    data->w = srmConnectorModeGetWidth(mode);
    data->h = srmConnectorModeGetHeight(mode);

    setupShaders(connector, userData);

    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, data->w, data->h);

    // Draw color squares
    drawColorSquares(data->w, data->h);

    // Set the pixels of the HW cursor
    srmConnectorSetCursor(connector, cursorPixels);

    // Schedule a repaint
    srmConnectorRepaint(connector);

    data->msStart = getMilliseconds();
}

static void paintGL(SRMConnector *connector, void *userData)
{
    struct ConnectorUserData *data = userData;

    if (buffer)
        glBindTexture(GL_TEXTURE_2D, srmBufferGetTextureID(data->rendererDevice, buffer));

    float cosine = cosf(data->phase);
    float sine = sinf(data->phase);

    // Draw color squares
    drawColorSquares(data->w, data->h);

    // Moving black vertical line
    glScissor((data->w-5)*(1.f+cosine)/2, 0, 10, data->h);
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Update cursor pos
    Int32 x = ((data->w - 64)/2)*(1 + cosine);
    Int32 y = ((data->h - 64)/2)*(1 + sine);
    srmConnectorSetCursorPos(connector, x, y);

    // Increase phase
    data->phase += 0.015f;

    if (data->phase >= 2*M_PI)
        data->phase -= 2*M_PI;

    srmConnectorRepaint(connector);
}

static void resizeGL(SRMConnector *connector, void *userData)
{
    struct ConnectorUserData *data = userData;

    SRMConnectorMode *mode = srmConnectorGetCurrentMode(connector);
    data->w = srmConnectorModeGetWidth(mode);
    data->h = srmConnectorModeGetHeight(mode);

    glViewport(0,
               0,
               data->w,
               data->h);

    srmConnectorRepaint(connector);
}

static void pageFlipped(SRMConnector *connector, void *userData)
{
    struct ConnectorUserData *data = userData;

    data->framesCount++;
    UInt64 msDiff = getMilliseconds() - data->msStart;
    UInt64 currSec = msDiff / 1000;

    if (currSec != data->sec && currSec % 5 == 0)
    {
        data->sec = currSec;
        SRMLog("srm-all-connectors: Connector (%d) FPS: %d.",
               srmConnectorGetID(connector),
               (data->framesCount * 1000) / msDiff);
    }
}

static void uninitializeGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(connector);

    struct ConnectorUserData *data = userData;

    if (data->program)
    {
        if (data->vertexShader)
            glDetachShader(data->program, data->vertexShader);

        if (data->fragmentShader)
            glDetachShader(data->program, data->fragmentShader);

        glDeleteProgram(data->program);
    }

    if (data->vertexShader)
        glDeleteShader(data->vertexShader);

    if (data->fragmentShader)
        glDeleteShader(data->fragmentShader);

    free(data);
}

static SRMConnectorInterface connectorInterface =
{
    .initializeGL = &initializeGL,
    .paintGL = &paintGL,
    .resizeGL = &resizeGL,
    .pageFlipped = &pageFlipped,
    .uninitializeGL = &uninitializeGL
};

static void initConnector(SRMConnector *connector)
{
    struct ConnectorUserData *userData = calloc(1, sizeof(struct ConnectorUserData));
    SRMDevice *device = srmConnectorGetDevice(connector);
    userData->rendererDevice = srmDeviceGetRendererDevice(device);

    if (!srmConnectorInitialize(connector, &connectorInterface, userData))
    {
        /* Fails to init connector */

        free(userData);
    }

    /* Connector initialized! */

}

static void deviceCreatedEventHandler(SRMListener *listener, SRMDevice *device)
{
    SRM_UNUSED(listener);
    SRM_UNUSED(device);
}

static void deviceRemovedEventHandler(SRMListener *listener, SRMDevice *device)
{
    SRM_UNUSED(listener);
    SRM_UNUSED(device);
}

static void connectorPluggedEventHandler(SRMListener *listener, SRMConnector *connector)
{
    SRM_UNUSED(listener);

    /* Got a new connector, let's render on it */
    initConnector(connector);
}

static void connectorUnpluggedEventHandler(SRMListener *listener, SRMConnector *connector)
{
    SRM_UNUSED(listener);
    SRM_UNUSED(connector);

    /* The connnector is automatically uninitialized after this event (if connected)
     * so there is no need to call srmConnectorUninitialize() */
}

int main(void)
{
    setenv("SRM_DEBUG", "4", 1);
    setenv("SRM_EGL_DEBUG", "4", 1);

    SRMCore *core = srmCoreCreate(&srmInterface, NULL);

    if (!core)
    {
        SRMFatal("Failed to initialize SRM core.");
        return 1;
    }

    buffer = srmBufferCreateFromCPU(core, 3, 2, 3*4, bufferPixels, DRM_FORMAT_XBGR8888);

    if (!buffer)
    {
        SRMWarning("Failed to create background texture buffer.");
    }

    // Set the pixels of the cursor white
    memset(cursorPixels, 255, sizeof(cursorPixels));

    // Subscribe to DRM events
    SRMListener *deviceCreatedEventListener = srmCoreAddDeviceCreatedEventListener(core, &deviceCreatedEventHandler, NULL);
    SRMListener *deviceRemovedEventListener = srmCoreAddDeviceRemovedEventListener(core, &deviceRemovedEventHandler, NULL);
    SRMListener *connectorPluggedEventListener = srmCoreAddConnectorPluggedEventListener(core, &connectorPluggedEventHandler, NULL);
    SRMListener *connectorUnpluggedEventListener = srmCoreAddConnectorUnpluggedEventListener(core, &connectorUnpluggedEventHandler, NULL);

    SRMListForeach (deviceIt, srmCoreGetDevices(core))
    {
        SRMDevice *device = srmListItemGetData(deviceIt);

        SRMListForeach (connectorIt, srmDeviceGetConnectors(device))
        {
            SRMConnector *connector = srmListItemGetData(connectorIt);

            if (srmConnectorIsConnected(connector) && srmConnectorGetmmWidth(connector) != 0)
                initConnector(connector);
        }
    }

    while (1)
    {
        /* Evdev monitor poll DRM devices/connectors hotplugging events (-1 disables timeout).
         * To get a pollable FD use srmCoreGetMonitorFD() */

        if (srmCoreProccessMonitor(core, 5) < 0)
            break;

        // Update the background texture every second
        if (buffer)
        {
            for (Int32 i = 0; i < (Int32)sizeof(bufferPixels);i++)
                bufferPixels[i] = rand() % 256;

            srmBufferWrite(buffer, 4*3, 0, 0, 3, 2, bufferPixels);
        }
    }


    /* Unsubscribe to DRM events
     *
     * These listeners are automatically destroyed when calling srmCoreDestroy()
     * so there is no need to free them manually.
     * This is here just to show how to unsubscribe to events on the fly. */

    srmListenerDestroy(deviceCreatedEventListener);
    srmListenerDestroy(deviceRemovedEventListener);
    srmListenerDestroy(connectorPluggedEventListener);
    srmListenerDestroy(connectorUnpluggedEventListener);

    // Finish SRM
    srmCoreDestroy(core);

    return 0;
}
