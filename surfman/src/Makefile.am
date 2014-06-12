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

DBUS_CLIENT_IDLS=
DBUS_SERVER_IDLS=surfman

CPROTO=cproto
INCLUDES =  ${LIBSURFMAN_INC} \
		${LIBVGAEMU_INC} \
	    ${DBUS_CFLAGS} \
	    ${PNG_CFLAGS} \
	    ${LIBXENSTORE_INC} \
	    ${LIBXENBACKEND_INC} \
	    ${LIBXC_INC} \
	    ${LIBDMBUS_INC} \
	    ${LIBEDID_INC} \
	    ${LIBPCIACCESS_INC} \
		${LIBPCIEMU_INC}

noinst_HEADERS=project.h prototypes.h

bin_PROGRAMS = surfman

SRCS=surfman.c \
     domain.c \
     dbus_glue.c \
     dbus.c \
     util.c \
     lockfile.c \
     surface.c \
     xenstore-helper.c \
     vgpu.c \
     plugin.c \
     resolution.c \
     xenfb.c \
     ioemugfx.c \
     snapshot.c \
     display.c \
     rpc.c \
     xengfx.c \
     splashscreen.c \
     fbtap.c

surfman_SOURCES = ${SRCS}
surfman_LDADD =  ${LIBSURFMAN_LIB} \
		 ${LIBVGAEMU_LIB} \
		 ${DBUS_LIBS} \
		 ${LIBXENSTORE_LIB} \
		 ${CLIENT_LIB} \
		 ${PNG_LIBS} \
		 ${DL_LIB} \
		 ${LIBXENBACKEND_LIB} \
		 ${LIBXCDBUS_LIBS} \
		 ${LIBDMBUS_LIB} \
		 ${LIBV4V_LIB} \
		 ${LIBXC_LIBS} \
		 ${LIBEDID_LIB} \
		 ${LIBPCIACCESS_LIB} \
		 ${LIBPCIEMU_LIB}

AM_CFLAGS=-g -W -Wall -Wno-unused -Werror -D_GNU_SOURCE

surfman_LDFLAGS=-lm

protos:
	echo > prototypes.h
	${CPROTO} -v -e -E "${CPP} ${CPPFLAGS}" -DPROTOS -v `echo "${INCLUDES}" | sed "s,[ ^]-[^I][^[:blank:]]*,,g"` ${SRCS} > prototypes.tmp
	mv -f prototypes.tmp prototypes.h

