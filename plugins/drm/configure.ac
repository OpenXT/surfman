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

AC_INIT([drm-plugin], [2.0])
AM_INIT_AUTOMAKE
LT_INIT

# Check for programs
AC_PROG_CC
AM_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_CHECK_PROG(MD5SUM, md5sum, md5sum)
AC_CHECK_PROG(GREP, grep, grep)

# Check for headers
AC_CHECK_HEADERS([stdio.h stdlib.h stdint.h stdarg.h unistd.h fcntl.h endian.h string.h strings.h errno.h assert.h dlfcn.h execinfo.h syslog.h])
AC_CHECK_HEADERS([sys/mman.h sys/ioctl.h sys/types.h sys/user.h])
AC_HEADER_TIME
AC_HEADER_ASSERT

# Check for specific structures/declarations
AC_STRUCT_TM

# Check compiler characteristics.
AC_C_INLINE
AC_C_CONST
AC_SYS_LARGEFILE

# Output files
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([src/config.h])

# Check for packages (pkgconfig support required).
PKG_CHECK_MODULES(LIBDRM, libdrm, [], [AC_MSG_ERROR("libdrm required.")])
AC_SUBST(LIBDRM_CFLAGS)
AC_SUBST(LIBDRM_LIBS)
PKG_CHECK_MODULES(LIBSURFMAN, libsurfman, [], [AC_MSG_ERROR("libsurfman required.")])
AC_SUBST(LIBSURFMAN_CFLAGS)
AC_SUBST(LIBSURFMAN_LIBS)
PKG_CHECK_MODULES(LIBUDEV, libudev, [], [AC_MSG_ERROR("libudev required.")])
AC_SUBST(LIBUDEV_CFLAGS)
AC_SUBST(LIBUDEV_LIBS)

## This one doesn't have pkg-config support...
AC_CHECK_HEADERS([event.h], [], [AC_MSG_ERROR("libevent required.")])

AC_CONFIG_FILES([Makefile 
	         src/Makefile ])
AC_OUTPUT
