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
* **SRM_FORCE_GL_ALLOCATION**=1<br>
Use OpenGL for buffer allocation instead of GBM, which may not always work.
* **SRM_RENDER_MODE_DUMB_FB_COUNT**=3<br>
Enable triple buffering in multi-GPU setups.

## DRM API

SRM defaults to using the Atomic DRM API for all devices (when avaliable), which may occasionally result in delayed hardware cursor updates. To enforce the use of the legacy API for all devices, simply set the following variable to 1.

**SRM_FORCE_LEGACY_API**=1

Note: Disabling vsync for the atomic API is supported only starting from Linux version 6.8.

## Device Blacklisting

To disable specific DRM devices, list the devices separated by ":", for example:

**SRM_DEVICES_BLACKLIST**=/dev/dri/card0:/dev/dri/card1

## Allocator Device

SRM attempts to automatically select the most suitable device for buffer allocation. However, certain drivers may incorrectly report capabilities or lack certain functionalities, leading to allocation failures or the creation of black, empty textures.

To override the default allocator, employ:

**SRM_ALLOCATOR_DEVICE**=/dev/dri/card[N]

## Main Memory Buffers

By default, SRM uses GBM for buffer allocation from main memory, resorting to OpenGL buffer allocation if GBM fails. Nonetheless, some drivers may erroneously report successful GBM buffer allocation when it actually fails. To enforce the use of OpenGL buffer allocation, employ:

**SRM_FORCE_GL_ALLOCATION**=1

## Render Buffering

You can customize the framebuffer count for both "ITSELF" and "DUMB" render modes using the following environment variables:

**SRM_RENDER_MODE_ITSELF_FB_COUNT**=N

**SRM_RENDER_MODE_DUMB_FB_COUNT**=N

Where N can be 1 = Single, 2 = Double or 3 = Triple buffering.

Please note that the "CPU" mode always uses single buffering, and this setting cannot be changed.

By default, the framebuffer count for each render mode is as follows:

* **ITSELF**: 2
* **DUMB**: 2
* **CPU**: 1

Remember to adjust the values accordingly based on your specific requirements and hardware capabilities.
