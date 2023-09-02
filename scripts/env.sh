#!/bin/bash

export SRM_DEB_PACKAGE_NAME=liblouvre-dev
export SRM_RPM_PACKAGE_NAME=liblouvre-devel
export SRM_DEB_PACKAGE_ARCH=amd64
export SRM_RPM_PACKAGE_ARCH=x86_64
export SRM_VERSION=`cat ./../VERSION`
export SRM_BUILD=`cat ./../BUILD`
export SRM_CHANGES=`cat ./../CHANGES`

