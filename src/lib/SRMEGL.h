#ifndef SRMEGL_H
#define SRMEGL_H

#include "SRMTypes.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Common EGL extensions */
struct SRMEGLCoreExtensionsStruct
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
};

/* Device specific EGL extensions */
struct SRMEGLDeviceExtensionsStruct
{
    UInt8 KHR_image;
    UInt8 KHR_image_base;
    UInt8 EXT_image_dma_buf_import;
    UInt8 EXT_image_dma_buf_import_modifiers;
    UInt8 EXT_create_context_robustness;
    UInt8 MESA_device_software;
#ifdef EGL_DRIVER_NAME_EXT
    UInt8 EXT_device_persistent_id;
#endif
    UInt8 EXT_device_drm;
    UInt8 EXT_device_drm_render_node;
    UInt8 KHR_no_config_context;
    UInt8 MESA_configless_context;
    UInt8 KHR_surfaceless_context;
    UInt8 IMG_context_priority;
};

/* Common EGL functions */
struct SRMEGLCoreFunctionsStruct
{
    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
    PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT;
    PFNEGLQUERYDEVICESTRINGEXTPROC eglQueryDeviceStringEXT;
    PFNEGLQUERYDISPLAYATTRIBEXTPROC eglQueryDisplayAttribEXT;
    PFNEGLDEBUGMESSAGECONTROLKHRPROC eglDebugMessageControlKHR;
};

/* Device specific EG functions */
struct SRMEGLDeviceFunctionsStruct
{
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
    PFNEGLQUERYDMABUFFORMATSEXTPROC eglQueryDmaBufFormatsEXT;
    PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
};

const char *srmEGLGetErrorString(EGLint error);
UInt8 srmEGLHasExtension(const char *extensions, const char *extension);

#ifdef __cplusplus
}
#endif

#endif // SRMEGL_H
