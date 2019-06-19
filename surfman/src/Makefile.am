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

AM_CFLAGS = \
	-D_GNU_SOURCE \
	$(DBUS_CFLAGS) \
	$(PNG_CFLAGS) \
	$(LIBSURFMAN_CFLAGS) \
	$(LIBXENBACKEND_CFLAGS) \
	$(LIBARGO_CFLAGS) \
	$(LIBDMBUS_CFLAGS) \
	$(LIBEDID_CFLAGS) \
	$(LIBPCIACCESS_CFLAGS) \
	$(LIBEVENT_CFLAGS)

noinst_HEADERS = project.h prototypes.h
bin_PROGRAMS = surfman

surfman_SOURCES = \
	surfman.c \
	domain.c \
	dbus_glue.c \
	dbus.c \
	lockfile.c \
	surface.c \
	xenstore-helper.c \
	plugin.c \
	resolution.c \
	xenfb.c \
	ioemugfx.c \
	snapshot.c \
	display.c \
	rpc.c \
	splashscreen.c \
	fbtap.c

surfman_LDADD =	\
	$(DBUS_LIBS) \
	$(PNG_LIBS) \
	$(LIBSURFMAN_LIBS) \
	$(LIBXENBACKEND_LIBS) \
	$(LIBARGO_LIBS) \
	$(LIBDMBUS_LIBS) \
	$(LIBEDID_LIBS) \
	$(LIBPCIACCESS_LIBS) \
	$(LIBEVENT_LIBS)

