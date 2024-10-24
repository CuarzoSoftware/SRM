#ifndef SRMEGL_H
#define SRMEGL_H

#include <SRMTypes.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMEGL SRMEGL
 *
 * @brief Utility EGL functions
 *
 * EGL is an interface between Khronos rendering APIs and the underlying native platform windowing system.
 * This module provides access to common and device-specific EGL extensions and functions.
 *
 * @see srmCoreGetEGLExtensions()
 * @see srmCoreGetEGLFunctions()
 * @see srmDeviceGetEGLExtensions()
 * @see srmDeviceGetEGLFunctions()
 * @{
 */

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

/**
 * @brief Structure representing common EGL extensions.
 */
typedef struct SRMEGLCoreExtensionsStruct
{
    UInt8 EXT_platform_base;
    UInt8 KHR_platform_gbm;
    UInt8 MESA_platform_gbm;
    UInt8 EXT_platform_device;
    UInt8 KHR_display_reference;
    UInt8 EXT_device_base;
    UInt8 EXT_device_enumeration;
    UInt8 EXT_device_query;
    UInt8 KHR_debug;
} SRMEGLCoreExtensions;

/**
 * @brief Structure representing common EGL functions.
 */
typedef struct SRMEGLCoreFunctionsStruct
{
    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
    PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT;
    PFNEGLQUERYDEVICESTRINGEXTPROC eglQueryDeviceStringEXT;
    PFNEGLQUERYDISPLAYATTRIBEXTPROC eglQueryDisplayAttribEXT;
    PFNEGLDEBUGMESSAGECONTROLKHRPROC eglDebugMessageControlKHR;
} SRMEGLCoreFunctions;

/**
 * @brief Structure representing device-specific EGL extensions.
 */
typedef struct SRMEGLDeviceExtensionsStruct
{
    UInt8 KHR_image;
    UInt8 KHR_image_base;
    UInt8 EXT_image_dma_buf_import;
    UInt8 EXT_image_dma_buf_import_modifiers;
    UInt8 EXT_create_context_robustness;
    UInt8 MESA_device_software;
    UInt8 KHR_image_pixmap;
    UInt8 KHR_gl_texture_2D_image;
    UInt8 KHR_gl_renderbuffer_image;
#ifdef EGL_DRIVER_NAME_EXT
    UInt8 EXT_device_persistent_id;
#endif
    UInt8 EXT_device_drm;
    UInt8 EXT_device_drm_render_node;
    UInt8 KHR_no_config_context;
    UInt8 MESA_configless_context;
    UInt8 KHR_surfaceless_context;
    UInt8 IMG_context_priority;
    UInt8 KHR_fence_sync;
    UInt8 ANDROID_native_fence_sync;
} SRMEGLDeviceExtensions;

/**
 * @brief Structure representing device-specific EGL functions.
 */
typedef struct SRMEGLDeviceFunctionsStruct
{
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
    PFNEGLQUERYDMABUFFORMATSEXTPROC eglQueryDmaBufFormatsEXT;
    PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
    PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES;
    PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
    PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
    PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
    PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR;
    PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID;
} SRMEGLDeviceFunctions;

/**
 * @brief Structure representing device-specific OpenGL extensions.
 */
typedef struct SRMGLDeviceExtensionsStruct
{
    UInt8 EXT_read_format_bgra;
    UInt8 EXT_texture_format_BGRA8888;
    UInt8 OES_EGL_image;
    UInt8 OES_EGL_image_base;
    UInt8 OES_EGL_image_external;
    UInt8 OES_EGL_sync;
    UInt8 OES_surfaceless_context;
} SRMGLDeviceExtensions;

/**
 * @brief Get a human-readable error string for an EGL error code.
 *
 * @param error An EGL error code.
 *
 * @return A pointer to a string describing the EGL error.
 */
const char *srmEGLGetErrorString(EGLint error);

/**
 * @brief Check if an EGL extension is supported.
 *
 * @param extensions A string containing EGL extensions.
 * @param extension The name of the extension to check.
 *
 * @return 1 if the extension is supported, 0 otherwise.
 */
UInt8 srmEGLHasExtension(const char *extensions, const char *extension);

/**
 * @brief Get a human-readable string for an EGL context priority.
 */
const char *srmEGLGetContextPriorityString(EGLint priority);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMEGL_H
