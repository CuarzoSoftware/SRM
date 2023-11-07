# 🏠 Simple Rendering Manager

SRM is a C library that simplifies the development of Linux DRM/KMS applications.

The main problem that this library aims to solve is the ability to render on all displays connected to a machine, regardless of whether their are controlled by different GPUs, and share textures between them from a single allocation. This is particularly useful, for example, in Wayland compositors where it is usually necessary to display the graphic buffers of applications on multiple screens at a time.

With SRM, you can focus on the OpenGL ES 2.0 logic of your application. For each available display, you can start a rendering thread that triggers common events like [initializeGL()](struct_s_r_m_connector_interface.html), [paintGL()](struct_s_r_m_connector_interface.html), [resizeGL()](struct_s_r_m_connector_interface.html), [pageFlip()](struct_s_r_m_connector_interface.html) and [uninitializeGL()](struct_s_r_m_connector_interface.html).

SRM allows you to use multiple GPUs simultaneously and automatically finds the most efficient configuration. It also offers functions for creating OpenGL textures, which are automatically shared among GPUs.

### Tested on

* Intel GPUs (i915 driver)
* NVIDIA GPUs (Nouveau and propietary drivers)
* Mali GPUs (Lima driver)

### Features

* Multiple GPUs support
* Automatic GPUs/connectors configuration
* Automatic texture sharing between GPUs
* Texture allocation from CPU buffers, DMA buffers, GBM BOs, Flink Handles, Wayland DRM buffers.
* Multi-session support ([libseat](https://github.com/kennylevinsen/seatd) can be used to open DRM devices for example)
* Connectors hot-plugging event listener
* Hardware cursor compositing
* V-Sync
* Frame buffer damage (improves performance in multi-GPUs setups when DMA is not supported)
* Access to screen framebuffers as textures.

### Multi-GPU Buffer Sharing and Rendering

Automatic buffer sharing across GPUs is accomplished through DMA. When all GPUs support it, each one can render into its own connectors (ITSELF MODE).

In cases where a GPU cannot import DMA buffers from the allocator GPU, another GPU handles the rendering for its connectors using DUMB buffers (DUMB MODE) or CPU copying (CPU MODE) as a last resort.

Performance in the last two modes can be significantly improved by specifying rects with the damaged regions after a `paintGL()` event using `srmConnectorSetBufferDamage()`.

> Note: You don't need to handle buffer allocation or rendering differently depending on the mode, all of this is managed internally by SRM.

### Upcoming Features

We are continuously working to enhance the capabilities of SRM. Here are some exciting upcoming features:

1. **System Profile Configuration Tool:** We are developing a tool that will generate a system profile configuration. This configuration can be used to override device parameters in faulty DRM drivers, providing more flexibility and robustness in handling hardware configurations.

2. **Asynchronous SRMBuffers:** We will be introducing new functions to create and write to SRMBuffers in asynchronous mode. This feature will improve the efficiency and responsiveness of SRM when dealing with buffers, especially in scenarios where asynchronous operations are critical.