#
# Copyright (c) 2012 Citrix Systems, Inc.
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
AC_PREREQ(2.13)
AC_INIT(src/surfman.c)

LT_INIT

LIBSURFMAN_MAJOR_VERSION=`cat $srcdir/version-major`
LIBSURFMAN_MINOR_VERSION=`cat $srcdir/version-minor`
LIBSURFMAN_MICRO_VERSION=`cat $srcdir/version-micro`

LIBSURFMAN_INTERFACE_VERSION_MIN=$LIBSURFMAN_MINOR_VERSION
LIBSURFMAN_INTERFACE_VERSION_MAX=$LIBSURFMAN_MINOR_VERSION
LIBSURFMAN_INTERFACE_REVISION=1

LIBSURFMAN_VERSION=$LIBSURFMAN_MAJOR_VERSION.$LIBSURFMAN_MINOR_VERSION.$LIBSURFMAN_MICRO_VERSION

AC_SUBST(LIBSURFMAN_MAJOR_VERSION)
AC_SUBST(LIBSURFMAN_MINOR_VERSION)
AC_SUBST(LIBSURFMAN_MICRO_VERSION)
AC_SUBST(LIBSURFMAN_VERSION)

AC_SUBST(LIBSURFMAN_INTERFACE_VERSION_MIN)
AC_SUBST(LIBSURFMAN_INTERFACE_VERSION_MAX)
AC_SUBST(LIBSURFMAN_INTERFACE_REVISION)

LT_RELEASE=$LIBSURFMAN_MAJOR_VERSION.$LIBSURFMAN_MINOR_VERSION
LT_CURRENT=$LIBSURFMAN_INTERFACE_VERSION_MAX
LT_REVISION=$LIBSURFMAN_MICRO_VERSION
LT_AGE=`expr $LIBSURFMAN_INTERFACE_VERSION_MAX - $LIBSURFMAN_INTERFACE_VERSION_MIN`

AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

VERSION=$LIBSURFMAN_VERSION
PACKAGE=libsurfman

AM_INIT_AUTOMAKE($PACKAGE,$VERSION)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
PKG_PROG_PKG_CONFIG([0.22])
AC_PROG_AWK
AC_CHECK_PROG(MD5SUM, md5sum, md5sum)
AC_CHECK_PROG(GREP, grep, grep)
AC_CHECK_PROG(SED, sed, sed)

# Checks for header files.
AC_CHECK_HEADERS([unistd.h fcntl.h errno.h stdlib.h stdint.h stropts.h syslog.h string.h stdio.h stdarg.h ctype.h])
AC_CHECK_HEADERS([sys/types.h sys/stat.h sys/mman.h])
AC_CHECK_HEADERS([pthread.h])

AC_CHECK_HEADERS([xen/xen.h linux/ioctl.h])
AC_CHECK_HEADERS([xen/sys/privcmd.h], [], [
        AC_MSG_ERROR([*** Xen specific header missing])
], [
#include <stdint.h>
#include <linux/ioctl.h>
#include <xen/xen.h>
])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_CONST
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for libraries.

ORIG_LIBS="${LIBS}"
LIBS=""
AC_SEARCH_LIBS([pthread_mutex_lock], [pthread])
PTHREAD_LIB="${LIBS}"
LIBS="${ORIG_LIBS}"
AC_SUBST(PTHREAD_LIB)

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

AC_SUBST(LIBXC_INC)
AC_SUBST(LIBXC_LIB)

ORIG_LDFLAGS="${LDFLAGS}"
ORIG_CPPFLAGS="${CPPFLAGS}"
LDFLAGS="${LDFLAGS} ${LIBXC_LIB}"
CPPFLAGS="${CPPFLAGS} ${LIBXC_INC}"
AC_CHECK_HEADERS([xenctrl.h])
AC_CHECK_FUNCS([xc_version])
LDFLAGS="${ORIG_LDFLAGS}"
CPPFLAGS="${ORIG_CPPFLAGS}"

# Check for libevent

AC_ARG_WITH([libevent],
            AC_HELP_STRING([--with-libevent=PATH], [Path to prefix where libevent (or libev) is installed]),
            [LIBEVENT_PREFIX=$with_libevent], [])

case "x$LIBEVENT_PREFIX" in
        x|xno|xyes)
                LIBEVENT_INC=""
                LIBEVENT_LIB="-levent"
                ;;
        *)
                LIBEVENT_INC="-I${LIBEVENT_PREFIX}/include"
                LIBEVENT_LIB="-L${LIBEVENT_PREFIX}/lib -levent"
                ;;
esac

AC_SUBST(LIBEVENT_INC)
AC_SUBST(LIBEVENT_LIB)

have_libevent=true

ORIG_LDFLAGS="${LDFLAGS}"
ORIG_CPPFLAGS="${CPPFLAGS}"
        LDFLAGS="${LDFLAGS} ${LIBEVENT_LIB}"
        CPPFLAGS="${CPPFLAGS} ${LIBEVENT_INC}"
        AC_CHECK_HEADERS([event.h], [], [have_libevent=false])
        AC_CHECK_FUNC([event_init], [], [have_libevent=false])
LDFLAGS="${ORIG_LDFLAGS}"
CPPFLAGS="${ORIG_CPPFLAGS}"

if test "x$have_libevent" = "xfalse"; then
        AC_MSG_ERROR([
*** At least libev or libevent is required.                
])
fi

# Output files.
AC_CONFIG_MACRO_DIR([m4])
AM_CONFIG_HEADER(src/config.h)

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/surfman-head.h
                 libsurfman.pc.src])
AC_CONFIG_FILES([libsurfman-config.src],
                [chmod +x libsurfman-config.src])

AC_OUTPUT
