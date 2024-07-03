# ⚙️ Environment Variables

## Debugging

To adjust the logging message level, configure the following environment variables:

**SRM_DEBUG**=N

**SRM_EGL_DEBUG**=N

Where N can be 0 = Disabled, 1 = Fatal, 2 = Error, 3 = Warning or 4 = Debug.

## Nvidia Configuration

Nvidia cards often work out of the box with nouveau, but this isn't always the case with proprietary drivers. A recommended configuration for proprietary drivers is:

* **SRM_FORCE_LEGACY_API**=0<br>
Use the Atomic DRM API if available.
* **SRM_NVIDIA_CURSOR**=0<br>
Disable cursor planes for nvidia_drm only, which often cause FPS drops when updated.

## DRM API

SRM defaults to using the Atomic DRM API for all devices (when avaliable), which may occasionally result in delayed hardware cursor updates. To enforce the use of the legacy API for all devices, simply set the following variable to 1.

**SRM_FORCE_LEGACY_API**=1

## Device Blacklisting

To disable specific DRM devices, list the devices separated by ":", for example:

**SRM_DEVICES_BLACKLIST**=/dev/dri/card0:/dev/dri/card1

## Allocator Device

By default, SRM automatically selects the integrated GPU for buffer allocation. To override the default allocator, use:

**SRM_ALLOCATOR_DEVICE**=/dev/dri/card[N]

## Main Memory Buffers

SRM uses GBM for buffer allocation from main memory, resorting to OpenGL if GBM fails. To enforce the use of OpenGL employ:

**SRM_FORCE_GL_ALLOCATION**=1

## Render Buffering

All connectors use double buffering by default. You can customize the number of framebuffers used for each rendering mode using the following environment variables:

**SRM_RENDER_MODE_ITSELF_FB_COUNT**=N

**SRM_RENDER_MODE_PRIME_FB_COUNT**=N

**SRM_RENDER_MODE_DUMB_FB_COUNT**=N

**SRM_RENDER_MODE_CPU_FB_COUNT**=N

Where N can be 2 = Double or 3 = Triple buffering.

> Using triple buffering can offer a smoother experience by allowing a new frame to be rendered while a page flip is pending, however, it does require more resources.
