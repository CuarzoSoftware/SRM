#!/bin/bash

# Fix linking problems

set -e
mkdir -p ${DESTDIR}/etc/ld.so.conf.d
echo "$1" >${DESTDIR}/etc/ld.so.conf.d/srm.conf
/sbin/ldconfig -N ${DESTDIR}/
exit 0