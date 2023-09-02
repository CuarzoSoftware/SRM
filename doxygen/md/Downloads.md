# Downloads


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