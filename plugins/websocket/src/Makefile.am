#
# Copyright (c) 2011 Citrix Systems, Inc.
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

CPROTO = cproto
INCLUDES = ${LIBSURFMAN_INC}
AM_CFLAGS=-g -W -Werror -Wall -Wno-unused

noinst_HEADERS=project.h prototypes.h http_parser.h

plugindir = ${libdir}/surfman
plugin_LTLIBRARIES = websocket.la

SRCS=   websocket.c     \
        http_parser.c

websocket_la_SOURCES = ${SRCS}
websocket_la_LIBADD =  ${LIBSURFMAN_LIB}
websocket_la_LDFLAGS = -module 

protos:
	echo > prototypes.h
	${CPROTO} -v -e -E "${CPP} ${CPPFLAGS}" -DPROTOS -v ${INCLUDES} ${SRCS} > prototypes.tmp
	mv -f prototypes.tmp prototypes.h
