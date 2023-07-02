#ifndef SRMRENDERMODECOMMON_H
#define SRMRENDERMODECOMMON_H

#include <EGL/egl.h>
#include <xf86drmMode.h>
#include "../../SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

enum SRM_CURSOR_ATOMIC_CHANGE
{
    SRM_CURSOR_ATOMIC_CHANGE_VISIBILITY = 1,
    SRM_CURSOR_ATOMIC_CHANGE_POSITION = 2,
};

Int8  srmRenderModeCommonMatchConfigToVisual(EGLDisplay egl_display, EGLint visual_id, EGLConfig *configs, int count);
Int8  srmRenderModeCommonChooseEGLConfiguration(EGLDisplay egl_display, const EGLint *attribs, EGLint visual_id, EGLConfig *config_out);
void  srmRenderModeCommonPageFlipHandler(int, unsigned int, unsigned int, unsigned int, void *data);
UInt8 srmRenderModeCommonCreateCursor(SRMConnector *connector);
void srmRenderModeCommonDestroyCursor(SRMConnector *connector);
UInt8 srmRenderModeCommonWaitRepaintRequest(SRMConnector *connector);
void srmRenderModeCommitCursorChanges(SRMConnector *connector, drmModeAtomicReqPtr req);

#ifdef __cplusplus
}
#endif

#endif // SRMRENDERMODECOMMON_H
