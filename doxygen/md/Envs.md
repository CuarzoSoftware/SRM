# ⚙️ Environment Variables {#envs_page}

## Debugging

To adjust the logging verbosity level, configure the following environment variables:

**SRM_DEBUG**=N (default = 0)

**SRM_EGL_DEBUG**=N (default = 0)

Where N can be 0 = Disabled, 1 = Fatal, 2 = Error, 3 = Warning or 4 = Debug.

## DRM API

SRM defaults to using the Atomic DRM API for all devices (when avaliable). To enforce the use of the legacy API set the following variable to 1.

* **SRM_FORCE_LEGACY_API**=[0,1] (default = 0)

Writeback connectors are specialized virtual connectors designed to access pixel data from offscreen plane compositions. They are particularly useful for capturing and processing the final output of the display pipeline.

* **SRM_ENABLE_WRITEBACK_CONNECTORS**=[0,1] (default = 0)

## Cursor Planes

* **SRM_FORCE_LEGACY_CURSOR**=[0,1] (default = 1)<br>
The legacy cursor IOCTLs are used by default, even when the atomic API is enabled, as they allow the cursor to be updated asynchronously, providing a much smoother experience. However, on some drivers, this may cause the cursor to flicker.

* **SRM_NVIDIA_CURSOR**=[0,1] (default = 0)<br>
Cursor planes are disabled by default for proprietary NVIDIA drivers, as updating the cursor can sometimes cause screen stutter. Nouveau drivers are not affected by this option.

* **SRM_DISABLE_CURSOR**=[0,1] (default = 0)<br>
This setting can be used to disable cursor planes for all drivers.

## Device Blacklisting

To disable specific DRM devices, list the devices separated by ":", for example:

**SRM_DEVICES_BLACKLIST**=/dev/dri/card0:/dev/dri/card1

## Buffer Allocation

By default, SRM automatically selects the integrated GPU for buffer allocation. To override the default allocator, use:

**SRM_ALLOCATOR_DEVICE**=/dev/dri/card[N]

SRM uses GBM for buffer allocation from main memory, resorting to OpenGL when the former fails. To enforce the use of OpenGL employ:

**SRM_FORCE_GL_ALLOCATION**=1 (default = 0)

## Render Buffering

All connectors use double buffering by default. You can customize the number of framebuffers used for each rendering mode using the following environment variables:

**SRM_RENDER_MODE_ITSELF_FB_COUNT**=N

**SRM_RENDER_MODE_PRIME_FB_COUNT**=N

**SRM_RENDER_MODE_DUMB_FB_COUNT**=N

**SRM_RENDER_MODE_CPU_FB_COUNT**=N

Where N can be 2 = Double or 3 = Triple buffering.

> Triple buffering can provide a smoother experience by allowing a new frame to be rendered while a page flip is pending. However, it consumes more memory and power and can introduce some latency.

## Direct Scanout

Scanning out custom buffers is allowed by default (see `srmConnectorSetCustomScanoutBuffer()`). This allows, for example, compositors to directly present fullscreen application windows without needing to render them using OpenGL.

To disable it, set:

**SRM_DISABLE_CUSTOM_SCANOUT**=1 (default = 0)
