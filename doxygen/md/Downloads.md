# 📦 Downloads

## Pre-built Binaries

Pre-built binaries are provided for the following distributions. Please be aware that their versions may not always match the latest SRM release.

* **Arch** : [libsrm](https://aur.archlinux.org/packages/libsrm) - *Thanks to [@TrialnError](https://aur.archlinux.org/account/TrialnError)*.
* **Debian** : [libsrm](https://packages.debian.org/source/sid/libsrm) - *Thanks to [Sudip Mukherjee](https://github.com/sudipm-mukherjee)*.
* **Fedora** : [libsrm](https://copr.fedorainfracloud.org/coprs/ngompa/louvre) - *Thanks to [Neal Gompa](https://github.com/Conan-Kudo)*.
* **NixOS** : [srm-cuarzo](https://search.nixos.org/packages?channel=unstable&show=srm-cuarzo&from=0&size=50&sort=relevance&type=packages&query=srm) - *Thanks to [Marco Rebhan](https://github.com/2xsaiko)*.

## Manual Building

SRM depends on the following libraries:

* [hwinfo](https://github.com/vcrhonek/hwdata)
* [libdisplay-info](https://gitlab.freedesktop.org/emersion/libdisplay-info)
* libudev
* libdrm
* libgbm
* libEGL >= 1.5
* libGLESv2

The **srm-multi-session** example also require:

* libseat
* libinput

### Debian (Ubuntu, Linux Mint, etc)

To build SRM from a Debian based distribution please install the following packages:

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

### RedHat (Fedora, CentOS, openSUSE, etc)

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