#
# Copyright (c) 2013 Citrix Systems, Inc.
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

INCLUDES = ${LIBXC_INC}

SRCS = surfman.c mapcache.c io.c util.c backtrace.c xc.c rect.c configfile.c \
       surface.c
CPROTO = cproto

SURFMANSRCS=${SRCS}

noinst_HEADERS = project.h prototypes.h surfman-tail.h ext_prototypes.h

libsurfman_la_SOURCES = ${SURFMANSRCS}
libsurfman_la_CFLAGS = ${AM_CFLAGS} -W -Wno-unused
libsurfman_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE) \
	-export-dynamic

BUILT_SOURCES = surfman.h

lib_LTLIBRARIES = libsurfman.la

include_HEADERS = surfman.h
AM_CFLAGS = -g

VFD = ${srcdir}/..
VF = ${shell cat ${VFD}/version-files}
VFS = ${VF:%=${VFD}/%}
VCHK = ${shell cat ${VFS} | @MD5SUM@ | @AWK@ '{ print $$1 }' }
VNUM = ${shell @GREP@ ${VCHK} ${VFD}/version-md5sums | @AWK@ '{ print $$2 }'  }
VDEF = ${shell echo `cat ${VFD}/version-major`.`cat ${VFD}/version-minor`.`cat ${VFD}/version-micro` }

surfman.h: surfman-head.h ext_prototypes.h surfman-tail.h
	cat surfman-head.h ext_prototypes.h surfman-tail.h > surfman.h

protos:
	echo > prototypes.h
	echo > ext_prototypes.h
	cat surfman-head.h ext_prototypes.h surfman-tail.h > surfman.h
	${CPROTO} -e -v -E "${CPP} ${CPPFLAGS} -I." -DINT_PROTOS ${INCLUDES} ${SRCS} > prototypes.tmp
	mv -f prototypes.tmp prototypes.h
	${CPROTO} -e -v -E "${CPP} ${CPPFLAGS} -I." -DEXT_PROTOS ${INCLUDES} ${SRCS} > ext_prototypes.h
	cat surfman-head.h ext_prototypes.h surfman-tail.h > surfman.h

version.h: $(VFD)/version-files $(VFD)/version-major \
	$(VFD)/version-minor $(VFD)/version-micro \
	$(VFD)/version-md5sums ${VFS} Makefile
	if [ .${VNUM} = . ]; then \
		echo "#define FULL_VERSION \"libsurfman Version ${VDEF} + Edits\"" > version.h; \
		echo ${VDEF}-E > version-num; \
	else \
		echo "#define FULL_VERSION \"libsurfman Version ${VNUM}\"" > version.h; \
		echo ${VNUM} > version-num; \
	fi
