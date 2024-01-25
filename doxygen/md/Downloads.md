# 📦 Downloads

SRM depends on the following libraries:

* [hwinfo](https://github.com/vcrhonek/hwdata)
* [libdisplay-info](https://gitlab.freedesktop.org/emersion/libdisplay-info)
* libudev
* libdrm
* libgbm
* libEGL
* libGLESv2

The **srm-multi-session** example also require:

* libseat
* libinput

## Debian (Ubuntu, Linux Mint, etc)

Thanks to [@sudipm-mukherjee](https://github.com/sudipm-mukherjee), you can directly install the library (v0.4.0-1) and examples from the Debian sid repository using the following command:

```bash
$ sudo apt install libsrm-dev libsrm-examples
```

To build SRM manually from a Debian based distribution please install the following packages:

```bash
$ sudo apt install build-essential meson libseat-dev libinput-dev libudev-dev libdrm-dev libgbm-dev libegl1-mesa-dev libgles2-mesa-dev hwinfo libdisplay-info-dev
```

If the [hwinfo](https://github.com/vcrhonek/hwdata) or [libdisplay-info](https://gitlab.freedesktop.org/emersion/libdisplay-info) packages are not available in your distribution, please download and install them manually in the specified order:

1. [hwinfo](https://packages.ubuntu.com/focal/hwdata)
2. [libdisplay-info0](https://packages.ubuntu.com/lunar/libdisplay-info0)
3. [libdisplay-info-dev](https://packages.ubuntu.com/lunar/libdisplay-info-dev)

Next, execute the following commands:

```bash
$ git clone https://github.com/CuarzoSoftware/SRM.git
$ cd SRM/src
$ meson setup build
$ cd build
$ meson install
$ sudo ldconfig
```

To ensure that everything is functioning correctly, you can test one of the available [examples](md_md__examples.html).

## RedHat (Fedora, CentOS, openSUSE, etc)

To build SRM from a RedHat based distribution please install the following packages:

```bash
$ sudo dnf install @development-tools
$ sudo dnf install meson hwinfo libseat-devel mesa-libEGL-devel libglvnd-devel libudev-devel libdrm-devel libgbm-devel libdisplay-info-devel libinput-devel
```

Next, execute the following commands:

```bash
$ git clone https://github.com/CuarzoSoftware/SRM.git
$ cd SRM/src
$ meson setup build
$ cd build
$ meson install
$ sudo ldconfig
```

To ensure that everything is functioning correctly, you can test one of the available [examples](md_md__examples.html).