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

AC_INIT([websocket-surfman], [0.1])
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE

AC_CONFIG_MACRO_DIR([m4])
AM_PROG_LIBTOOL

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_AWK
AC_CHECK_PROG(MD5SUM,md5sum,md5sum)
AC_CHECK_PROG(GREP,grep,grep)

AC_SYS_LARGEFILE

AC_CONFIG_HEADERS([config.h])

PKG_CHECK_MODULES([LIBSURFMAN], [libsurfman])
LIBSURFMAN_INC="$LIBSURFMAN_CFLAGS"
LIBSURFMAN_LIB="$LIBSURFMAN_LIBS"

AC_SUBST(LIBSURFMAN_INC)
AC_SUBST(LIBSURFMAN_LIB)

PKG_CHECK_MODULES([LIBXCIDC], [libxcidc])
LIBXCIDC_INC="$LIBXCIDC_CFLAGS"
LIBXCIDC_LIB="$LIBXCIDC_LIBS"

AC_SUBST(LIBXCIDC_INC)
AC_SUBST(LIBXCIDC_LIB)

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

