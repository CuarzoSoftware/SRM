# 🏠 Simple Rendering Manager

SRM is a C library that simplifies the development of Linux DRM/KMS applications.

The main problem that this library aims to solve is the ability to render on all displays connected to a machine, regardless of whether their are controlled by different GPUs, and share textures between them from a single allocation. This is particularly useful, for example, in Wayland compositors where it is usually necessary to display the graphic buffers of applications on multiple screens at a time.

With SRM, you can focus on the OpenGL ES 2.0 logic of your application. For each available display, you can start a rendering thread that triggers common events like [initializeGL()](struct_s_r_m_connector_interface.html), [paintGL()](struct_s_r_m_connector_interface.html), [resizeGL()](struct_s_r_m_connector_interface.html), [pageFlip()](struct_s_r_m_connector_interface.html) and [uninitializeGL()](struct_s_r_m_connector_interface.html).

SRM allows you to use multiple GPUs simultaneously and automatically finds the most efficient configuration. It also offers functions for creating OpenGL textures, which are automatically shared among GPUs.