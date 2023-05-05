#include <private/SRMBufferPrivate.h>
#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>

#include <SRMList.h>
#include <SRMLog.h>

#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <gbm.h>
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>


SRMBuffer *srmBufferCreateFromCPU(SRMCore *core, UInt32 width, UInt32 height, UInt8 *pixels, SRM_BUFFER_FORMAT format)
{
    SRMBuffer *buffer = calloc(1, sizeof(SRMBuffer));
    buffer->core = core;
    buffer->allocatorBO = gbm_bo_create(core->allocatorDevice->gbm, width, height, format, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);

    if (!buffer->allocatorBO)
    {
        SRMError("Failed to create buffer from CPU with allocator device %s.", core->allocatorDevice->name);
        goto fail;
    }

    buffer->allocatorBOMap = gbm_bo_map(buffer->allocatorBO,
                                        0,
                                        0,
                                        gbm_bo_get_width(buffer->allocatorBO),
                                        gbm_bo_get_height(buffer->allocatorBO),
                                        GBM_BO_TRANSFER_WRITE,
                                        &buffer->allocatorBOStride,
                                        &buffer->allocatorBOMapData);

    if (buffer->allocatorBOMap == MAP_FAILED)
    {
        SRMError("Failed to map buffer created from CPU with allocator device %s.", core->allocatorDevice->name);
        buffer->allocatorBOMap = NULL;
        goto fail;
    }

    memcpy(buffer->allocatorBOMap, pixels, width*height*4);


    buffer->textures = srmListCreate();

    return buffer;

    fail:
    srmBufferDestroy(buffer);
    return NULL;
}

GLuint srmBufferGetTextureID(SRMDevice *device, SRMBuffer *buffer)
{
    // Check if already created
    struct SRMBufferTexture *texture;
    SRMListForeach(item, buffer->textures)
    {
        texture = srmListItemGetData(item);

        if (texture->device == device)
            return texture->texture;
    }

    // Creates the texture
    texture = calloc(1, sizeof(struct SRMBufferTexture));
    texture->device = device;
    texture->image = eglCreateImage(device->eglDisplay, device->eglSharedContext, EGL_NATIVE_PIXMAP_KHR, buffer->allocatorBO, NULL);

    if (texture->image == EGL_NO_IMAGE)
    {
        SRMError("Failed to create EGL image from GBM BO.");
    }

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &texture->texture);
    glBindTexture(GL_TEXTURE_2D, texture->texture);
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress("glEGLImageTargetTexture2DOES");
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, texture->image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    srmListAppendData(buffer->textures, texture);

    SRMDebug("Created GLES2 Texture from BO.");

    return texture->texture;
}

void srmBufferDestroy(SRMBuffer *buffer)
{
    if (buffer->textures)
    {
        srmListDestoy(buffer->textures);
    }

    if (buffer->allocatorBO)
    {
        if (buffer->allocatorBOMap)
            gbm_bo_unmap(buffer->allocatorBO, buffer->allocatorBOMapData);

        gbm_bo_destroy(buffer->allocatorBO);
    }

    free(buffer);
}
