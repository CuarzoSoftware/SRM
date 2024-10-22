#include <SRMEGL.h>
#include <string.h>

const char *srmEGLGetErrorString(EGLint error)
{
    switch (error)
    {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_DEVICE_EXT:
            return "EGL_BAD_DEVICE_EXT";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
    }
    return "unknown error";
}

UInt8 srmEGLHasExtension(const char *extensions, const char *extension)
{
    size_t extlen = strlen(extension);
    const char *end = extensions + strlen(extensions);

    while (extensions < end)
    {
        if (*extensions == ' ')
        {
            extensions++;
            continue;
        }

        size_t n = strcspn(extensions, " ");

        if (n == extlen && strncmp(extension, extensions, n) == 0)
            return 1;

        extensions += n;
    }

    return 0;
}

const char *srmEGLGetContextPriorityString(EGLint priority)
{
    switch (priority)
    {
    case EGL_CONTEXT_PRIORITY_HIGH_IMG:
        return "HIGH";
    case EGL_CONTEXT_PRIORITY_MEDIUM_IMG:
        return "MEDIUM";
    case EGL_CONTEXT_PRIORITY_LOW_IMG:
        return "LOW";
    default:
        return "UNKNOWN";
    }
}
