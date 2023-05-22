#include <private/modes/SRMRenderModeCommon.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMConnectorPrivate.h>

#include <SRMLog.h>
#include <stdlib.h>

// Choose EGL configurations

Int8 srmRenderModeCommonMatchConfigToVisual(EGLDisplay egl_display, EGLint visual_id, EGLConfig *configs, int count)
{
    for (int i = 0; i < count; ++i)
    {
        EGLint id;

        if (!eglGetConfigAttrib(egl_display, configs[i], EGL_NATIVE_VISUAL_ID, &id))
            continue;

        if (id == visual_id)
            return i;
    }

    return -1;
}

Int8  srmRenderModeCommonChooseEGLConfiguration(EGLDisplay egl_display, const EGLint *attribs, EGLint visual_id, EGLConfig *config_out)
{
    EGLint count = 0;
    EGLint matched = 0;
    EGLConfig *configs;
    int config_index = -1;

    if (!eglGetConfigs(egl_display, NULL, 0, &count) || count < 1)
    {
        SRMError("No EGL configs to choose from.");
        return 0;
    }

    configs = (void**)malloc(count * sizeof *configs);

    if (!configs)
        return 0;

    if (!eglChooseConfig(egl_display, attribs, configs, count, &matched) || !matched)
    {
        SRMError("No EGL configs with appropriate attributes.");
        goto out;
    }

    if (!visual_id)
    {
        config_index = 0;
    }

    if (config_index == -1)
    {
        config_index = srmRenderModeCommonMatchConfigToVisual(egl_display, visual_id, configs, matched);
    }

    if (config_index != -1)
    {
        *config_out = configs[config_index];
    }

out:
    free(configs);
    if (config_index == -1)
        return 0;

    return 1;
}

void srmRenderModeCommonPageFlipHandler(int a, unsigned int b, unsigned int c, unsigned int d, void *data)
{
    SRM_UNUSED(a);
    SRM_UNUSED(b);
    SRM_UNUSED(c);
    SRM_UNUSED(d);
    SRMConnector *connector = data;
    connector->pendingPageFlip = 0;
}

UInt8 srmRenderModeCommonCreateCursor(SRMConnector *connector)
{
    connector->cursorBO = gbm_bo_create(connector->device->gbm,
                                        64,
                                        64,
                                        GBM_FORMAT_ARGB8888,
                                        GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);

    return connector->cursorBO != NULL;
}

UInt8 srmRenderModeCommonWaitRepaintRequest(SRMConnector *connector)
{
    if (!connector->repaintRequested)
    {
        pthread_mutex_lock(&connector->repaintMutex);
        pthread_cond_wait(&connector->repaintCond, &connector->repaintMutex);
        pthread_mutex_unlock(&connector->repaintMutex);
    }

    if (connector->state == SRM_CONNECTOR_STATE_UNINITIALIZING)
    {
        connector->interface->uninitializeGL(connector, connector->interfaceData);
        connector->renderInterface.uninitialize(connector);
        connector->state = SRM_CONNECTOR_STATE_UNINITIALIZED;
        return 0;
    }

    return 1;
}
