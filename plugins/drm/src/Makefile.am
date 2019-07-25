#
# Copyright (c) 2014 Citrix Systems, Inc.
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

SRCS =	drm-plugin.c			\
	device.c			\
	device-intel.c			\
	framebuffer-dumb.c		\
	framebuffer-i915_foreign.c	\
	monitor.c			\
	udev.c				\
	hotplug.c			\
	backlight.c

HDRS = drm-plugin.h utils.h list.h project.h prototypes.h

plugindir = ${libdir}/surfman
plugin_LTLIBRARIES = drm-plugin.la

drm_plugin_la_SOURCES = ${SRCS} ${HDRS}
drm_plugin_la_LDFLAGS = -module
drm_plugin_la_CFLAGS = ${LIBUDEV_CFLAGS} ${LIBDRM_CFLAGS} ${LIBSURFMAN_CFLAGS} -W -Wall -Werror -g
drm_plugin_la_CPPFLAGS = ${LIBUDEV_CFLAGS} ${LIBDRM_CFLAGS} ${LIBSURFMAN_CFLAGS}
drm_plugin_la_LIBADD = ${LIBUDEV_LIBS} ${LIBDRM_LIBS} ${LIBSURFMAN_LIBS}

protos: 
	echo > prototypes.h
	${CPROTO} -e -v -E "${CPP} $(drm_plugin_la_CPPFLAGS)" -DINT_PROTOS -v ${INCLUDES} ${SRCS} > prototypes.tmp
	mv -f prototypes.tmp prototypes.h
