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
#include <SRMBuffer.h>

#include <SRMList.h>
#include <SRMLog.h>

#include <GLES2/gl2.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static UInt8 cursor[64*64*4];

static UInt8 bufferPixels[3*2*4] =
{
    255, 0, 0, 255,     // Red
    0, 255, 0, 255,     // Green
    0, 0, 255, 255,     // Blue
    255, 255, 0, 255,   // Yellow
    255, 0, 255, 255,   // Pink
    0, 255, 255, 255,   // Cyan
};

static SRMBuffer *buffer;

// Square (left for vertex, right for fragment)
static GLfloat square[16] =
{  //  VERTEX       FRAGMENT
    -1.0f, -1.0f,   0.f, 1.f, // TL
    -1.0f,  1.0f,   0.f, 0.f, // BL
     1.0f,  1.0f,   1.f, 0.f, // BR
     1.0f, -1.0f,   1.f, 1.f  // TR
};

static int openRestricted(const char *path, int flags, void *userData)
{
    SRM_UNUSED(userData);

    // Here something like libseat could be used instead
    return open(path, flags);
}

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

static void setupShaders(SRMConnector *connector)
{
    const char *vertex_shader_source = "attribute vec4 position; varying vec2 v_texcoord; void main() { gl_Position = vec4(position.xy, 0.0, 1.0); v_texcoord = position.zw; }";
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);

    // Check for vertex shader compilation errors
    GLint success = 0;
    GLchar infoLog[512];
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        glGetShaderInfoLog(vertex_shader, 512, NULL, infoLog);
        SRMError("Vertex shader compilation error: %s.", infoLog);
    }

    const char *fragment_shader_source = "precision mediump float; uniform sampler2D tex; varying vec2 v_texcoord; void main() { gl_FragColor = texture2D(tex, v_texcoord); }";
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);

    success = 0;
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragment_shader, 512, NULL, infoLog);
        SRMError("Fragment shader compilation error: %s.", infoLog);
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glUseProgram(program);

    // Bind vPosition to attribute 0
    glBindAttribLocation(program, 0, "position");

    // Load the vertex data
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, square);

    // Enables the vertex array
    glEnableVertexAttribArray(0);

    if (buffer)
    {
        SRMDevice *device = srmConnectorGetDevice(connector);
        SRMDevice *rendererDevice = srmDeviceGetRendererDevice(device);
        glBindTexture(GL_TEXTURE_2D, srmBufferGetTextureID(rendererDevice, buffer));
    }

}

static void initializeGL(SRMConnector *connector, void *data)
{
    setupShaders(connector);

    SRMConnectorMode *mode = srmConnectorGetCurrentMode(connector);
    UInt32 w = srmConnectorModeGetWidth(mode);
    UInt32 h = srmConnectorModeGetHeight(mode);

    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, w, h);

    // Draw color squares
    drawColorSquares(w, h);

    // Create a gray cursor
    memset(cursor, 200, sizeof(cursor));
    srmConnectorSetCursor(connector, cursor);

    float *phase = (float*)data;
    *phase = 0;

    srmConnectorRepaint(connector);
}

static void paintGL(SRMConnector *connector, void *userData)
{
    SRMConnectorMode *mode = srmConnectorGetCurrentMode(connector);
    UInt32 w = srmConnectorModeGetWidth(mode);
    UInt32 h = srmConnectorModeGetHeight(mode);

    float *phase = (float*)userData;
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
    srmConnectorSetCursorPos(connector, x, y);

    // Increase phase
    *phase += 0.015f;

    if (*phase >= 2*M_PI)
        *phase -= 2*M_PI;

    srmConnectorRepaint(connector);
}

static void resizeGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(userData);

    SRMConnectorMode *mode = srmConnectorGetCurrentMode(connector);
    UInt32 w = srmConnectorModeGetWidth(mode);
    UInt32 h = srmConnectorModeGetHeight(mode);

    glViewport(0,
               0,
               w,
               h);

    srmConnectorRepaint(connector);

    SRMLog("RESIZE GL");
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
    .uninitializeGL = &uninitializeGL
};

static void initConnector(SRMConnector *connector)
{
    float *phase = malloc(sizeof(float));

    if (!srmConnectorInitialize(connector, &connectorInterface, phase))
    {
        free(phase);
        SRMError("Failed to initialize device %s connector %d.",
                 srmDeviceGetName(srmConnectorGetDevice(connector)),
                 srmConnectorGetID(connector));
    }

    SRMDebug("Initialized connector %s.", srmConnectorGetModel(connector));
}

static void deviceCreatedEventHandler(SRMListener *listener, SRMDevice *device)
{
    SRM_UNUSED(listener);

    SRMDebug("DRM device (GPU) created: %s.", srmDeviceGetName(device));
}

static void deviceRemovedEventHandler(SRMListener *listener, SRMDevice *device)
{
    SRM_UNUSED(listener);

    SRMDebug("DRM device (GPU) removed: %s.", srmDeviceGetName(device));
}

static void connectorPluggedEventHandler(SRMListener *listener, SRMConnector *connector)
{
    SRM_UNUSED(listener);

    SRMDebug("DRM connector PLUGGED (%d): %s.",
                  srmConnectorGetID(connector),
                  srmConnectorGetModel(connector));

    initConnector(connector);
}

static void connectorUnpluggedEventHandler(SRMListener *listener, SRMConnector *connector)
{
    SRM_UNUSED(listener);

    SRMDebug("DRM connector UNPLUGGED (%d): %s.",
                  srmConnectorGetID(connector),
                  srmConnectorGetModel(connector));

    /* The connnector is automatically uninitialized after this event */
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

    buffer = srmBufferCreateFromCPU(core, 3, 2, bufferPixels, SRM_BUFFER_FORMAT_ABGR8888);

    if (!buffer)
    {
        SRMWarning("Failed to create texture buffer.");
    }

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
        // Poll DRM devices/connectors hotplugging events (0 disables timeout)
        if (srmCoreProccessMonitor(core, -1) < 0)
            break;
    }


    // Unsubscribe to DRM events
    //
    // These listeners are automatically destroyed when calling srmCoreDestroy()
    // so there is no need to free them manually.
    // This is here just to show how to unsubscribe to events on the fly.
    srmListenerDestroy(deviceCreatedEventListener);
    srmListenerDestroy(deviceRemovedEventListener);
    srmListenerDestroy(connectorPluggedEventListener);
    srmListenerDestroy(connectorUnpluggedEventListener);

    // Finish SRM
    srmCoreDestroy(core);

    return 0;
}
