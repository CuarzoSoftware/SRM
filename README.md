# Simple Rendering Manager

SRM is a C++ library that simplifies the development of Linux DRM/KMS API applications.

> Still under development ⚠️

The main problem that this library aims to solve is the ability to render on all displays connected to a machine, regardless of whether their are controlled by different GPUs, and share textures between them from a single allocation. This is particularly useful, for example, in Wayland compositors where it is usually necessary to display the graphic buffers of applications on multiple screens at a time.

With SRM, you can focus on the OpenGL ES 2.0 logic of your application. For each available display, you can start a rendering thread that triggers common events like **initializeGL()**, **paintGL()**, **resizeGL()**, etc.

SRM allows you to use multiple GPUs simultaneously and automatically finds the most efficient configuration. It also offers functions for creating OpenGL textures, which are automatically shared among GPUs when utilized (you only have to create a texture once).

### Features

* Multiple GPUs setup
* Automatic optimal GPUs/connectors configuration
* Automatic texture sharing between GPUs
* Multi seat support (libseat can be used to open DRM devices for example)
* GPU hot-plugging events
* Connectors hot-plugging events
* Hardware cursor compositing
* V-Sync

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
$ cd SRM
$ meson setup build
$ cd build
$ sudo meson install
```

### Automatic Configuration

Here are the steps in which SRM finds the best configuration:

1. Obtains the resources and capabilities of each GPU.
2. Determines which GPU will be used as an allocator, giving priority to allowing as many GPUs as possible to access the same texture through DMA.
3. Identifies which GPUs are capable of rendering (the ones that can import textures from the allocator GPU).
4. If a GPU cannot import textures from the allocator, a renderer GPU is assigned to render for it.
5. In that case, to display the rendered buffer from the renderer GPU on the connector of the non renderer GPU, SRM prioritizes the use of DUMB BUFFERS and, as a last resort, CPU copying.
6. When initiating a rendering thread on a connector, SRM searches for the best possible combination of ENCODER, CRTC, and PRIMARY PLANE.
7. SRM also looks for a CURSOR PLANE, which, if available, can assign its pixels and position using the setCursor() and setCursorPos() functions.
8. If no CURSOR PLANE is found because they are all being used by other connectors, the CURSOR PLANE will be automatically added to the connector that needs it once one of those connectors is deinitialized.

### Connectors Render Modes

These are the possible connectors rendering modes form the best case to the worst case:

1. ITSELF (the connector's GPU can directly render into it)
2. DMA (the connector' GPU exports DMA buffers and another GPU renders directly into it)
3. DUMB (the connector' GPU creates dumb buffers and glReadPixels is used to copy the buffers rendered by another GPU)
4. CPU (renderer GPU > CPU (glReadPixels) > connector GPU (glTexImage2D) > render)
