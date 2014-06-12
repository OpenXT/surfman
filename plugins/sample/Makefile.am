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

noinst_HEADERS=project.h prototypes.h

plugindir = ${libdir}/surfman
plugin_LTLIBRARIES = sample.la

SRCS=   sample.c

sample_la_SOURCES = ${SRCS}
sample_la_LIBADD =  ${LIBSURFMAN_LIB}
sample_la_LDFLAGS = -module 

protos:
	echo > prototypes.h
	${CPROTO} -v -e -E "${CPP} ${CPPFLAGS}" -DPROTOS -v ${INCLUDES} ${SRCS} > prototypes.tmp
	mv -f prototypes.tmp prototypes.h
