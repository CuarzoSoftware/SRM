#ifndef SRMEGLPRIVATE_H
#define SRMEGLPRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <EGL/egl.h>

static const EGLint commonEGLConfigAttribs[] =
{
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 0,
    EGL_NONE
};

#ifdef __cplusplus
}
#endif

#endif // SRMEGLPRIVATE_H
