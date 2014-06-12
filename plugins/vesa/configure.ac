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

AC_INIT([vesa], [0.1])
AC_CONFIG_SRCDIR([vesa.c])
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

AC_ARG_WITH(libxc,
            AC_HELP_STRING([--with-libxc=PATH],
                           [Path to prefix where where libxc and xen were installed]),
            LIBXC_PREFIX=$with_libxc, [])

case "x$LIBXC_PREFIX" in
        x|xno|xyes)
                LIBXC_INC=""
                LIBXC_LIB="-lxenctrl"
                ;;
        *)
                LIBXC_INC="-I${LIBXC_PREFIX}/include"
                LIBXC_LIB="-L${LIBXC_PREFIX}/lib -lxenctrl"
                ;;
esac

have_libxenctrl=true

ORIG_LDFLAGS="${LDFLAGS}"
ORIG_CPPFLAGS="${CPPFLAGS}"
LDFLAGS="${LDFLAGS} ${LIBXC_LIB}"
CPPFLAGS="${CPPFLAGS} ${LIBXC_INC}"
AC_CHECK_HEADERS([xenctrl.h])
AC_CHECK_FUNCS([xc_version])
LDFLAGS="${ORIG_LDFLAGS}"
CPPFLAGS="${ORIG_CPPFLAGS}"

AC_SUBST(LIBXC_INC)
AC_SUBST(LIBXC_LIB)

PKG_CHECK_MODULES([LIBSURFMAN], [libsurfman])
LIBSURFMAN_INC="$LIBSURFMAN_CFLAGS"
LIBSURFMAN_LIB="$LIBSURFMAN_LIBS"

AC_SUBST(LIBSURFMAN_INC)
AC_SUBST(LIBSURFMAN_LIB)

PKG_CHECK_MODULES(X11, x11)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

