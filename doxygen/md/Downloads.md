# 📦 Downloads

## Pre-built Binaries

Pre-built binaries are provided for the following distributions. Please be aware that their versions may not always match the latest SRM release.

* **Arch** : [libsrm](https://aur.archlinux.org/packages/libsrm) - *Thanks to [@TrialnError](https://aur.archlinux.org/account/TrialnError)*.
* **Arch** : [libsrm-devel-git](https://aur.archlinux.org/packages/libsrm-devel-git) - *Thanks to [@kingdomkind ](https://github.com/kingdomkind) (devel branch)*.
* **Debian** : [libsrm](https://packages.debian.org/source/sid/libsrm) - *Thanks to [Sudip Mukherjee](https://github.com/sudipm-mukherjee)*.
* **Fedora** : [cuarzo-srm](https://copr.fedorainfracloud.org/coprs/ehopperdietzel/cuarzo/) - *By [Eduardo Hopperdietzel](https://github.com/ehopperdietzel) (always up to date)*.
* **NixOS** : [srm-cuarzo](https://search.nixos.org/packages?channel=unstable&show=srm-cuarzo&from=0&size=50&sort=relevance&type=packages&query=srm) - *Thanks to [Marco Rebhan](https://github.com/2xsaiko)*.

## Manual Building

SRM depends on the following libraries:

* libudev >= 249
* libdrm >= 2.4.113
* gbm >= 23.2.1
* egl >= 1.5
* gl >= 1.2
* glesv2 >= 3.2
* hwinfo
* libdisplay-info

The **srm-multi-session** example also require:

* libseat >= 0.6.4
* libinput >= 1.20.0

### Debian (Ubuntu, Linux Mint, etc)

To build SRM from a Debian based distribution please install the following packages:

```bash
$ sudo apt install build-essential meson libseat-dev libinput-dev libudev-dev libdrm-dev libgbm-dev libegl1-mesa-dev libgles2-mesa-dev hwinfo libdisplay-info-dev
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
