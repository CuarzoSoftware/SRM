#ifndef SRMRENDERMODECOMMON_H
#define SRMRENDERMODECOMMON_H

#include <EGL/egl.h>
#include <xf86drmMode.h>
#include <SRMTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

enum SRM_ATOMIC_CHANGE
{
    SRM_ATOMIC_CHANGE_CURSOR_VISIBILITY = 1 << 0,
    SRM_ATOMIC_CHANGE_CURSOR_POSITION   = 1 << 1,
    SRM_ATOMIC_CHANGE_CURSOR_BUFFER     = 1 << 2,
    SRM_ATOMIC_CHANGE_GAMMA_LUT         = 1 << 3
};

Int8  srmRenderModeCommonMatchConfigToVisual(EGLDisplay egl_display, EGLint visual_id, EGLConfig *configs, int count);
Int8  srmRenderModeCommonChooseEGLConfiguration(EGLDisplay egl_display, const EGLint *attribs, EGLint visual_id, EGLConfig *config_out);
void  srmRenderModeCommonPageFlipHandler(Int32 fd, UInt32 seq, UInt32 sec, UInt32 usec, void *data);
UInt8 srmRenderModeCommonCreateCursor(SRMConnector *connector);
void srmRenderModeCommonDestroyCursor(SRMConnector *connector);
UInt8 srmRenderModeCommonWaitRepaintRequest(SRMConnector *connector);
void srmRenderModeCommitAtomicChanges(SRMConnector *connector, drmModeAtomicReqPtr req);
Int32 srmRenderModeAtomicResetConnectorProps(SRMConnector *connector);
Int32 srmRenderModeAtomicCommit(Int32 fd, drmModeAtomicReqPtr req, UInt32 flags, void *data);
Int32 srmRenderModeCommonInitCrtc(SRMConnector *connector, UInt32 fb);
void srmRenderModeCommonPauseRendering(SRMConnector *connector);
void srmRenderModeCommonResumeRendering(SRMConnector *connector, UInt32 fb);
void srmRenderModeCommonUninitialize(SRMConnector *connector);
Int32 srmRenderModeCommonUpdateMode(SRMConnector *connector, UInt32 fb);
void srmRenderModeCommonPageFlip(SRMConnector *connector, UInt32 fb);
void srmRenderModeCommonWaitPageFlip(SRMConnector *connector);

#ifdef __cplusplus
}
#endif

#endif // SRMRENDERMODECOMMON_H
