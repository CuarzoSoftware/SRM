# Downloads

SRM depends on the following libraries:

* [hwinfo](https://github.com/vcrhonek/hwdata)
* [libdisplay-info](https://gitlab.freedesktop.org/emersion/libdisplay-info)
* libudev
* libdrm
* libgbm
* libEGL
* libGLESv2

## Debian (Ubuntu, Linux Mint, etc)

#### Pre-built Binaries

To access the latest SRM .deb packages, please visit [releases (TODO)](/).

If the [libdisplay-info](https://gitlab.freedesktop.org/emersion/libdisplay-info) package is not available in your distribution, please download and install the following packages in the specified order:

1. [libdisplay-info0](https://packages.ubuntu.com/lunar/libdisplay-info0)
2. [libdisplay-info-dev](https://packages.ubuntu.com/lunar/libdisplay-info-dev)

For those interested in manually building SRM, please install the following packages:

```bash
$ sudo apt install libdisplay-info-dev libudev-dev libdrm-dev libgbm-dev libegl1-mesa-dev libgles2-mesa-dev
```

Next, execute the following commands:

```bash
$ cd SRM/src
$ meson setup build -Dbuildtype=custom
$ cd build
$ sudo meson install
```

## TODO: RedHat (Fedora, CentOS, openSUSE, etc)
## TODO: Arch (Manjaro, Artix, etc)

