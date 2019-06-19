#
# Autoconf initialisation.
#
AC_INIT(surfman, 2.1)
AC_CONFIG_HEADERS(src/config.h)
AC_PREREQ(2.13)
AC_CONFIG_SRCDIR(src/surfman.c)
AM_INIT_AUTOMAKE([foreign -Wall -Werror])

#
# Version management.
# +1    : 0     Interface changes breaking retro-compatibility.
# 0     : +1    Internal changes without lost of retro-compatibility.
#
VERSION_MAJOR=2
VERSION_MINOR=1
AC_DEFINE_UNQUOTED([VERSION_MAJOR], [$VERSION_MAJOR], [Surfman major version number])
AC_DEFINE_UNQUOTED([VERSION_MINOR], [$VERSION_MINOR], [Surfman minor version number])

#
# Standard checks.
#
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Compiler capabilities.
AC_C_INLINE
AC_C_CONST

# Checks for header files.
AC_CHECK_HEADERS([fb2if.h], [], AC_MSG_ERROR([fb2if header is missing.]))
AC_CHECK_HEADERS([fbtap.h], [], AC_MSG_ERROR([fbtap header is missing.]))

# Required modules.
PKG_CHECK_MODULES([DBUS],[dbus-1])
PKG_CHECK_MODULES([PNG], [libpng])
PKG_CHECK_MODULES([LIBEVENT], [libevent])
PKG_CHECK_MODULES([LIBSURFMAN], [libsurfman])
PKG_CHECK_MODULES([LIBXENBACKEND], [libxenbackend])
PKG_CHECK_MODULES([LIBARGO], [libargo])
PKG_CHECK_MODULES([LIBDMBUS], [libdmbus])
PKG_CHECK_MODULES([LIBEDID], [libedid])
PKG_CHECK_MODULES([LIBPCIACCESS], [pciaccess])

# Required libraries.
AC_SEARCH_LIBS([cos], [m])
AC_SEARCH_LIBS([dlopen], [dl dld])
AC_CHECK_LIB([xenstore], [xs_open])
AC_CHECK_LIB([xenctrl], [xc_interface_open])

# IDL definitions.
AC_ARG_WITH(idldir, AC_HELP_STRING([--with-idldir=PATH],
                                   [Path to dbus idl desription files]),
            IDLDIR=$with_idldir, IDLDIR=/usr/share/idl)
AC_SUBST(IDLDIR)

#
# Outputs.
#
AC_OUTPUT([Makefile
	   src/Makefile
	   app/Makefile
           ])

