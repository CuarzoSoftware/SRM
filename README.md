# Simple Rendering Manager

SRM is a C library that simplifies the development of Linux DRM/KMS API applications.

With SRM, you can focus on the OpenGL ES 2.0 logic of your application. For each available display, you can start a rendering thread that triggers common events like **initializeGL()**, **paintGL()**, **resizeGL()**, **pageFlipped()** and **uninitializeGL()**.

SRM allows you to use multiple GPUs simultaneously and automatically finds the most efficient configuration. It also offers functions for creating OpenGL textures, which are automatically shared among GPUs.

### Links

* [📖 C API Documentation](https://cuarzosoftware.github.io/SRM/modules.html)
* [🎓 Tutorial](https://cuarzosoftware.github.io/SRM/md_md__tutorial.html)
* [🕹️ Examples](https://cuarzosoftware.github.io/SRM/md_md__examples.html)
* [📦 Downloads](https://cuarzosoftware.github.io/SRM/md_md__downloads.html)
* [💬 Contact](https://cuarzosoftware.github.io/SRM/md_md__contact.html)


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

### Tested on

* Intel GPUs (i915 driver)
* NVIDIA GPUs (Nouveau and propietary drivers)
* Mali GPUs (Lima driver)


