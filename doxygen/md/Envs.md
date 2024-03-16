# ⚙️ Environment Variables

## Debugging

To enable debug logging messages, you can set the following variables:

**SRM_DEBUG**=[0, 1, 2, 3, 4]

**SRM_EGL_DEBUG**=[0, 1, 2, 3, 4]

Please note that debugging logging is disabled by default. By assigning a value from 0 to 4 to each of these variables, you can control the level of debug messages displayed. A higher value corresponds to more detailed debug information. Set these variables to 0 to disable debug logging if you no longer need the extra logging information.

## DRM API

SRM defaults to using the Atomic DRM API for all devices (when avaliable), which may occasionally result in delayed hardware cursor updates. To enforce the use of the legacy API for all devices, simply set the following variable to 1.

**SRM_FORCE_LEGACY_API**=1

Note: Disabling vsync for the atomic API is supported only starting from Linux version 6.8.

## Allocator Device

SRM attempts to automatically select the most suitable device for buffer allocation. However, certain drivers may incorrectly report capabilities or lack certain functionalities, leading to allocation failures or the creation of black, empty textures.

To override the default allocator, use the **SRM_ALLOCATOR_DEVICE** environment variable. For instance:

**SRM_ALLOCATOR_DEVICE**=/dev/dri/card1

## Main Memory Buffers

By default, SRM uses GBM for buffer allocation from main memory, resorting to OpenGL buffer allocation if GBM fails. Nonetheless, some drivers may erroneously report successful GBM buffer allocation when it actually fails. To enforce the use of OpenGL buffer allocation, employ:

**SRM_FORCE_GL_ALLOCATION**=1

## Render Buffering

You can customize the framebuffer count (single, double or triple buffering) for both "ITSELF" and "DUMB" render modes using the following environment variables:

**SRM_RENDER_MODE_ITSELF_FB_COUNT**=[1, 2, 3]

**SRM_RENDER_MODE_DUMB_FB_COUNT**=[1, 2, 3]

Please note that the "CPU" mode always uses single buffering, and this setting cannot be changed.

By default, the framebuffer count for each render mode is as follows:

* **ITSELF**: 2
* **DUMB**: 2
* **CPU**: 1

Remember to adjust the values accordingly based on your specific requirements and hardware capabilities.
