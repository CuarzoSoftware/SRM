# Simple Rendering Manager

SRM is a C library that simplifies the development of Linux DRM/KMS API applications.

The main problem that this library aims to solve is the ability to render on all displays connected to a machine, regardless of whether their are controlled by different GPUs, and share textures between them from a single allocation. This is particularly useful, for example, in Wayland compositors where it is usually necessary to display the graphic buffers of applications on multiple screens at a time.

With SRM, you can focus on the OpenGL ES 2.0 logic of your application. For each available display, you can start a rendering thread that triggers common events like **initializeGL()**, **paintGL()**, **resizeGL()**, **pageFlip()** and **uninitializeGL()**.

SRM allows you to use multiple GPUs simultaneously and automatically finds the most efficient configuration. It also offers functions for creating OpenGL textures, which are automatically shared among GPUs.

### Contact

If you have any questions or suggestions, please feel free to email me at <ehopperdietzel@gmail.com>.

### Features

* Multiple GPUs support
* Automatic optimal GPUs/connectors configuration
* Automatic texture sharing between GPUs
* Texture allocation from CPU buffers, DMA buffers, GBM BOs, Flink Handles, Wayland DRM buffers.
* Multi seat support (libseat can be used to open DRM devices for example)
* GPU hot-plugging event listener
* Connectors hot-plugging event listener
* Hardware cursor compositing
* V-Sync
* Frame buffer damage (improves performance when DMA is not supported)
* Access to renderbuffers as textures.

### Dependencies

* [hwinfo](https://github.com/vcrhonek/hwdata)
* [libdisplay-info](https://gitlab.freedesktop.org/emersion/libdisplay-info)
* libudev
* libdrm
* libgbm
* libEGL
* libGLESv2

### Build

```
$ cd SRM/src
$ meson setup build -Dbuildtype=custom
$ cd build
$ sudo meson install
```
### Examples

To learn the basics, refer to the **srm-basic** example. Additionally, for creating textures from a single allocation and setting the hardware cursor pixels and position, you can check the **srm-all-connectors** example.

### Tested on

* Intel GPUs (i915 driver)
* NVIDIA GPUs (Nouveau and propietary drivers)
* Mali GPUs (Lima driver)

### Connectors Render Modes

These are the possible connectors rendering modes form best to worst case:

1. ITSELF (the connector's GPU can directly render into it)
2. DUMB (the connector' GPU creates dumb buffers and DMA map or glReadPixels is used to copy the buffers rendered by another GPU)
3. CPU (renderer GPU > CPU (DMA map or glReadPixels) > connector GPU (glTexImage2D) > render)


### Framebuffer damage

DUMB and CPU modes can greatly benefit from receiving information about the changes occurring in the buffer within a frame, commonly known as "damage." By providing this damage information, we can optimize the performance of these modes.

To define the generated damage, after rendering a frame in paintGL(), you can add an array of rectangles (SRMRect) with the damaged areas using the srmConnectorSetBufferDamage() function. It is important to ensure that the coordinates of these rectangles originate from the top-left corner of the framebuffer and do not extend beyond its boundaries to avoid segmentation errors.

The ITSELF mode does not benefit from buffer damages, and therefore, calling the function in that case is a no-op. To determine if a connector supports buffer damages, you can use the srmConnectorHasBufferDamageSupport() method

### Automatic Configuration

Here are the steps in which SRM internally finds the best configuration:

1. Obtains the resources and capabilities of each GPU.
2. Determines which GPU will be used as an allocator, giving priority to allowing as many GPUs as possible to access textures through DMA.
3. Identifies which GPUs are capable of rendering (the ones that can import textures from the allocator GPU).
4. If a GPU cannot import textures from the allocator, another GPU is assigned to render for it.
5. In that case, to display the rendered buffer from the renderer GPU on the connector of the non renderer GPU, SRM prioritizes the use of DUMB BUFFERS and, as a last resort, CPU copying (all this is handled internally by SRM).
6. When initiating a rendering thread on a connector, SRM searches for the best possible combination of ENCODER, CRTC, and PRIMARY PLANE.
7. SRM also looks for a CURSOR PLANE, which, if available, can assign its pixels and position using the setCursor() and setCursorPos() functions.
8. If no CURSOR PLANE is found because they are all being used by other connectors, the CURSOR PLANE will be automatically added to the connector that needs it once one of those connectors is deinitialized.

### Environment Variables

You can customize the framebuffer count for both "ITSELF" and "DUMB" render modes using the following environment variables:

**SRM_RENDER_MODE_ITSELF_FB_COUNT**=[1, 2, 3]

**SRM_RENDER_MODE_DUMB_FB_COUNT**=[1, 2, 3]

If you set the value to 1 for any of these variables, it will disable v-sync.

Please note that v-sync is always disabled for the "CPU" mode, and this setting cannot be changed.

By default, the framebuffer count for each render mode is as follows:

* ITSELF: 2
* DUMB: 1
* CPU: 1

Remember to adjust the values accordingly based on your specific requirements and hardware capabilities.


