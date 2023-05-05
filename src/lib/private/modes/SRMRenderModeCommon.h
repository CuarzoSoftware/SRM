#ifndef SRMRENDERMODECOMMON_H
#define SRMRENDERMODECOMMON_H

#include <EGL/egl.h>
#include <SRMTypes.h>

Int8  srmRenderModeCommonMatchConfigToVisual(EGLDisplay egl_display, EGLint visual_id, EGLConfig *configs, int count);
Int8  srmRenderModeCommonChooseEGLConfiguration(EGLDisplay egl_display, const EGLint *attribs, EGLint visual_id, EGLConfig *config_out);
void  srmRenderModeCommonPageFlipHandler(int, unsigned int, unsigned int, unsigned int, void *data);
UInt8 srmRenderModeCommonCreateCursor(SRMConnector *connector);
UInt8 srmRenderModeCommonWaitRepaintRequest(SRMConnector *connector);

#endif // SRMRENDERMODECOMMON_H
