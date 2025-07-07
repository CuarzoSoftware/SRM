#if 1 == 2
#include <assert.h>
#include <private/SRMConnectorPrivate.h>
#include <private/SRMConnectorModePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMEncoderPrivate.h>
#include <private/SRMCrtcPrivate.h>
#include <private/SRMPlanePrivate.h>
#include <private/SRMCorePrivate.h>
#include <private/SRMBufferPrivate.h>

#include <private/modes/SRMRenderModeCommon.h>
#include <private/modes/SRMRenderModeItself.h>
#include <private/modes/SRMRenderModePrime.h>
#include <private/modes/SRMRenderModeDumb.h>
#include <private/modes/SRMRenderModeCPU.h>

#include <SRMLog.h>
#include <SRMList.h>

#include <stdio.h>
#include <unistd.h>
#include <xf86drmMode.h>
#include <libdisplay-info/edid.h>
#include <libdisplay-info/info.h>

#include <string.h>
#include <errno.h>
#include <stdlib.h>



// Find a valid (encoder,crtc and primary plane) trio and a cursor plane if avaliable
// Returns 1 if a valid trio is found and 0 otherwise
UInt8 srmConnectorGetBestConfiguration(SRMConnector *connector, SRMEncoder **bestEncoder, SRMCrtc **bestCrtc, SRMPlane **bestPrimaryPlane, SRMPlane **bestCursorPlane)
{
    int bestScore = 0;
    *bestEncoder = NULL;
    *bestCrtc = NULL;
    *bestPrimaryPlane = NULL;
    *bestCursorPlane = NULL;

    SRMListForeach (item1, connector->encoders)
    {
        SRMEncoder *encoder = srmListItemGetData(item1);

        SRMListForeach (item2, encoder->crtcs)
        {
            SRMCrtc *crtc = srmListItemGetData(item2);

            // Check if already used by other connector
            if (crtc->currentConnector)
                continue;

            int score = 0;
            SRMPlane *primaryPlane = NULL;
            SRMPlane *cursorPlane = NULL;

            // Check if has a cursor plane
            SRMListForeach (item3, connector->device->planes)
            {
                SRMPlane *plane = srmListItemGetData(item3);

                if (plane->type == SRM_PLANE_TYPE_OVERLAY)
                    continue;

                SRMListForeach (item4, plane->crtcs)
                {
                    SRMCrtc *planeCrtc = srmListItemGetData(item4);

                    // Check if already used by other connector
                    if (planeCrtc->currentConnector)
                        continue;

                    if (planeCrtc->id == crtc->id)
                    {
                        if (plane->type == SRM_PLANE_TYPE_PRIMARY)
                        {
                            primaryPlane = plane;
                            break;
                        }
                        else if (plane->type == SRM_PLANE_TYPE_CURSOR)
                        {
                            cursorPlane = plane;
                            break;
                        }
                    }
                }
            }

            // Can not render if no primary plane
            if (!primaryPlane)
                continue;

            score += 100;

            if (cursorPlane)
                score += 50;

            if (score > bestScore)
            {
                bestScore = score;
                *bestEncoder = encoder;
                *bestCrtc = crtc;
                *bestPrimaryPlane = primaryPlane;
                *bestCursorPlane = cursorPlane;
            }
        }
    }

    return *bestEncoder && *bestCrtc && *bestPrimaryPlane;
}

void *srmConnectorRenderThread(void *conn)
{
    SRMConnector *connector = conn;

    if (pthread_mutex_init(&connector->repaintMutex, NULL))
    {
        SRMError("[%s] [%s] Could not create render mutex.",
                 connector->device->shortName,
                 connector->name);
        goto finish;
    }
    else
        connector->hasRepaintMutex = 1;

    if (pthread_cond_init(&connector->repaintCond, NULL))
    {
        SRMError("[%s] [%s] Could not create render pthread_cond.",
                 connector->device->shortName,
                 connector->name);
        goto finish;
    }
    else
        connector->hasRepaintCond = 1;

    connector->pendingPageFlip = 0;
    connector->bufferAgeFrame = 0;
    connector->drmEventCtx.version = DRM_EVENT_CONTEXT_VERSION,
    connector->drmEventCtx.vblank_handler = NULL,
    connector->drmEventCtx.page_flip_handler = &srmRenderModeCommonPageFlipHandler,
    connector->drmEventCtx.page_flip_handler2 = NULL;
    connector->drmEventCtx.sequence_handler = NULL;

    srmRenderModeCommonCreateCursor(connector);

    SRM_RENDER_MODE renderMode = srmDeviceGetRenderMode(connector->device);

    SRMDebug("[%s] [%s] Rendering Mode: %s.",
             connector->device->shortName,
             connector->name,
             srmGetRenderModeString(renderMode));

    if (renderMode == SRM_RENDER_MODE_ITSELF)
        srmRenderModeItselfSetInterface(connector);
    else if (renderMode == SRM_RENDER_MODE_PRIME)
        srmRenderModePrimeSetInterface(connector);
    else if (renderMode == SRM_RENDER_MODE_DUMB)
        srmRenderModeDumbSetInterface(connector);
    else if (renderMode == SRM_RENDER_MODE_CPU)
        srmRenderModeCPUSetInterface(connector);
    else
    {
        assert(0 && "Invalid render mode for connector.");
    }

    SRMListForeach(devIt, connector->device->core->devices)
    {
        SRMDevice *dev = srmListItemGetData(devIt);

        if (dev == connector->device->rendererDevice)
            continue;

        if (!srmDeviceCreateSharedContextForThread(dev))
            goto finish;
    }

    if (!connector->renderInterface.initialize(connector))
    {
        SRMError("[%s] [%s] Render mode interface initialize() failed.",
                 connector->device->shortName,
                 connector->name);
        goto finish;
    }

    connector->renderInitResult = 1;

    // Render loop
    while (1)
    {
        connector->currentVSync = connector->pendingVSync;

        if (!srmRenderModeCommonWaitRepaintRequest(connector))
            break;

        pthread_mutex_lock(&connector->stateMutex);

paintGLChangedLockCurrentBuffer:

        if (connector->lockCurrentBuffer)
            connector->repaintRequested = 0;

        if (connector->state == SRM_CONNECTOR_STATE_INITIALIZED)
        {
            if (connector->repaintRequested)
            {
                    connector->repaintRequested = 0;

                    connector->inPaintGL = 1;
                    connector->renderInterface.render(connector);
                    connector->inPaintGL = 0;

                    if (connector->lockCurrentBuffer)
                        goto paintGLChangedLockCurrentBuffer;

                    /* Scanning out a custom user buffer (skip the render mode interface) */
                    if (connector->userScanoutBufferRef[0])
                    {
                        connector->bufferAgeFrame = 0;
                        srmRenderModeCommonDestroySync(connector);
                        srmRenderModeCommonPageFlip(connector, connector->userScanoutBufferRef[0]->scanout.fb);
                        connector->interface->pageFlipped(connector, connector->interfaceData);
                    }
                    /* Normal connector rendering */
                    else
                    {
                        connector->renderInterface.flipPage(connector);

                        if (connector->bufferAgeFrame < connector->renderInterface.getBuffersCount(connector))
                            connector->bufferAgeFrame++;
                    }

                    /* Release previous custom scanout buffer if any */
                    if (connector->userScanoutBufferRef[0] || connector->userScanoutBufferRef[1])
                    {
                        srmConnectorReleaseUserScanoutBuffer(connector, 1);
                        connector->userScanoutBufferRef[1] = connector->userScanoutBufferRef[0];
                        connector->userScanoutBufferRef[0] = NULL;
                    }

                    pthread_mutex_unlock(&connector->stateMutex);
                    continue;
            }
            else if (connector->atomicChanges)
            {
                srmRenderModeCommonPageFlip(connector, connector->lastFb);
            }
        }

        if (connector->state == SRM_CONNECTOR_STATE_CHANGING_MODE)
        {
            SRMWarning("[connector] Changing mode started.");

            connector->bufferAgeFrame = 0;

            if (connector->renderInterface.updateMode(connector))
            {
                connector->state = SRM_CONNECTOR_STATE_INITIALIZED;
                pthread_mutex_unlock(&connector->stateMutex);
                SRMWarning("[connector] Changing mode finished.");
                continue;
            }
            else
            {
                SRMFatal("[connector] Changing mode failed");
                connector->state = SRM_CONNECTOR_STATE_REVERTING_MODE;
                pthread_mutex_unlock(&connector->stateMutex);
                continue;
            }
        }
        else if (connector->state == SRM_CONNECTOR_STATE_SUSPENDING)
        {
            connector->state = SRM_CONNECTOR_STATE_SUSPENDED;
            connector->renderInterface.pause(connector);
            pthread_mutex_unlock(&connector->stateMutex);
            usleep(1000);
            SRMDebug("[%s] [%s] Paused.", connector->device->shortName, connector->name);
            continue;
        }
        else if (connector->state == SRM_CONNECTOR_STATE_RESUMING)
        {
            connector->state = SRM_CONNECTOR_STATE_INITIALIZED;
            connector->bufferAgeFrame = 0;
            connector->renderInterface.resume(connector);
            pthread_mutex_unlock(&connector->stateMutex);
            usleep(1000);
            SRMDebug("[%s] [%s] Resumed.", connector->device->shortName, connector->name);
            continue;
        }

        pthread_mutex_unlock(&connector->stateMutex);
    }

    finish:

    srmRenderModeCommonDestroySync(connector);

    SRMListForeach(devIt, connector->device->core->devices)
    {
        SRMDevice *dev = srmListItemGetData(devIt);

        if (dev == connector->device->rendererDevice)
            continue;

        srmDeviceDestroyThreadSharedContext(dev, pthread_self());
    }

    connector->renderInitResult = -1;
    return NULL;
}

void srmConnectorUnlockRenderThread(SRMConnector *connector, UInt8 repaint)
{
    connector->repaintRequested = repaint;
    pthread_cond_signal(&connector->repaintCond);
}

// This is called when a connector is uninitialized to assign
// its cursor plane to another initialized connector that needs it
void srmConnectorSetCursorPlaneToNeededConnector(SRMPlane *cursorPlane)
{
    if (cursorPlane->currentConnector)
        return;

    SRMListForeach(connectorIt, cursorPlane->device->connectors)
    {
        SRMConnector *connector = srmListItemGetData(connectorIt);

        if (srmConnectorGetState(connector) == SRM_CONNECTOR_STATE_INITIALIZED &&
            !srmConnectorHasHardwareCursor(connector))
        {
            SRMListForeach (crtcIt, cursorPlane->crtcs)
            {
                SRMCrtc *crtc = srmListItemGetData(crtcIt);

                if (crtc->id == connector->currentCrtc->id)
                {
                    cursorPlane->currentConnector = connector;
                    connector->currentCursorPlane = cursorPlane;
                    srmRenderModeCommonCreateCursor(connector);
                    return;
                }
            }
        }
    }
}

void srmConnectorRenderThreadCleanUp(SRMConnector *connector)
{
    connector->renderThread = 0;
    srmConnectorReleaseUserScanoutBuffer(connector, 0);
    srmConnectorReleaseUserScanoutBuffer(connector, 1);
    srmRenderModeCommonDestroyCursor(connector);

    if (connector->hasRepaintCond)
    {
        pthread_cond_destroy(&connector->repaintCond);
        connector->hasRepaintCond = 0;
    }

    if (connector->hasRepaintMutex)
    {
        pthread_mutex_destroy(&connector->repaintMutex);
        connector->hasRepaintCond = 0;
    }

    if (connector->currentCrtc)
    {
        connector->currentCrtc->currentConnector = NULL;
        connector->currentCrtc = NULL;
    }

    if (connector->currentEncoder)
    {
        connector->currentEncoder->currentConnector = NULL;
        connector->currentEncoder = NULL;
    }

    if (connector->currentPrimaryPlane)
    {
        connector->currentPrimaryPlane->currentConnector = NULL;
        connector->currentPrimaryPlane = NULL;
    }

    if (connector->currentCursorPlane)
    {
        connector->currentCursorPlane->currentConnector = NULL;
        srmConnectorSetCursorPlaneToNeededConnector(connector->currentCursorPlane);
        connector->currentCursorPlane = NULL;
    }

    connector->interfaceData = NULL;
    connector->interface = NULL;

    if (connector->gamma)
    {
        free(connector->gamma);
        connector->gamma = NULL;
    }

    if (connector->gammaBlobId)
    {
        drmModeDestroyPropertyBlob(connector->device->fd, connector->gammaBlobId);
        connector->gammaBlobId = 0;
    }

    if (connector->damageBoxes)
    {
        free(connector->damageBoxes);
        connector->damageBoxes = NULL;
        connector->damageBoxesCount = 0;
    }

    if (connector->currentModeBlobId)
    {
        drmModeDestroyPropertyBlob(connector->device->fd, connector->currentModeBlobId);
        connector->currentModeBlobId = 0;
    }
}

void srmConnectorInitGamma(SRMConnector *connector)
{
    if (!connector->currentCrtc)
        return;

    UInt64 gammaSize = srmCrtcGetGammaSize(connector->currentCrtc);

    if (gammaSize > 0)
    {
        SRMDebug("[%s] [%s] Gamma Size: %d.",
                 connector->device->shortName,
                 connector->name,
                 (UInt32)gammaSize);

        connector->gamma = malloc(gammaSize * sizeof(*connector->gamma));

        /* Apply linear gamma */

        Float64 n = gammaSize - 1.0;
        Float64 val;

        if (connector->device->clientCapAtomic)
        {
            struct drm_color_lut *lut = connector->gamma;

            for (UInt32 i = 0; i < gammaSize; i++)
            {
                val = (Float64)i / n;
                lut->red = lut->green = lut->blue = (UInt16)(UINT16_MAX * val);
                lut++;
            }

            connector->atomicChanges |= SRM_ATOMIC_CHANGE_GAMMA_LUT;
        }
        else
        {
            UInt16 *r = (UInt16*)connector->gamma;
            UInt16 *g = r + gammaSize;
            UInt16 *b = g + gammaSize;

            for (UInt32 i = 0; i < gammaSize; i++)
            {
                val = (Float64)i / n;
                r[i] = g[i] = b[i] = (UInt16)(UINT16_MAX * val);
            }

            if (drmModeCrtcSetGamma(connector->device->fd,
                                    connector->currentCrtc->id,
                                    (UInt32)gammaSize,
                                    r,
                                    g,
                                    b))
            {
                SRMError("[%s] [%s] Failed to set gamma using legacy API drmModeCrtcSetGamma().",
                         connector->device->shortName,
                         connector->name);
            }
        }
    }
    else
    {
        SRMDebug("[%s] [%s] Does not support gamma correction.",
                 connector->device->shortName,
                 connector->name);
    }
}

void srmConnectorReleaseUserScanoutBuffer(SRMConnector *connector, Int8 index)
{
    if (connector->userScanoutBufferRef[index])
    {
        srmBufferDestroy(connector->userScanoutBufferRef[index]);
        connector->userScanoutBufferRef[index] = NULL;
    }
}
#endif
