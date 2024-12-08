%global basever 0.11.0
%global origrel 1
%global somajor 0

Name:           cuarzo-srm
Version:        %{basever}%{?origrel:_%{origrel}}
Release:        1%{?dist}
Summary:        Simple Rendering Manager: C library for building OpenGL ES 2.0 applications on top of DRM/KMS

License:        LGPLv2.1
URL:            https://github.com/CuarzoSoftware/SRM

BuildRequires:  tar
BuildRequires:  wget
BuildRequires:  meson
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig(gl)
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(glesv2)
BuildRequires:  pkgconfig(libudev)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(gbm)
BuildRequires:  pkgconfig(libdisplay-info)

# Examples
BuildRequires:  pkgconfig(libinput)
BuildRequires:  pkgconfig(libseat)

%description
SRM is a C library that simplifies the development of Linux DRM/KMS
applications.

With SRM, you can focus on the OpenGL ES 2.0 logic of your application.
For each available display, you can start a rendering thread that triggers
common events like initializeGL(), paintGL(), resizeGL(), pageFlipped()
and uninitializeGL().

SRM allows you to use multiple GPUs simultaneously and automatically finds
the most efficient configuration. It also offers functions for creating OpenGL
textures, which are automatically shared among GPUs.


%package        devel
Summary:        Development files for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.

%package        examples
Summary:        Example applications using %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    examples
The %{name}-examples package contains example applications using
%{name}.

%prep
rm -rf repo
rm -f src.tar.gz
mkdir -p repo
wget -O src.tar.gz %{url}/archive/refs/tags/v%{basever}-%{origrel}.tar.gz
tar --strip-components=1 -xzvf src.tar.gz -C repo

%build
pushd repo/src
%meson
%meson_build

%install
pushd repo/src
%meson_install

%files
%license repo/LICENSE
%doc repo/BUILD repo/CHANGES repo/VERSION
%{_libdir}/libSRM.so.%{somajor}

%files examples
%{_bindir}/srm-all-connectors
%{_bindir}/srm-basic
%{_bindir}/srm-display-info
%{_bindir}/srm-multi-session
%{_bindir}/srm-direct-scanout

%files devel
%doc repo/README.md repo/doxygen
%{_includedir}/SRM/
%{_libdir}/libSRM.so
%{_libdir}/pkgconfig/SRM.pc

%changelog
* Sun Dec 08 2024 Eduardo Hopperdietzel <ehopperdietzel@gmail.com> - %{basever}-%{origrel}
- Updated license to LGPLv2.1.
- srmConnectorGetCurrentBufferAge: Retrieves the age of the current buffer according to the [EGL_EXT_buffer_age](https://registry.khronos.org/EGL/extensions/EXT/EGL_EXT_buffer_age.txt) extension specification.
- srmBufferWrite2(Begin/Update/End): An alternative to srmBufferWrite() that allows multiple texture updates without issuing an immediate internal synchronization.
