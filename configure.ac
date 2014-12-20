AC_PREREQ(2.57)
AC_INIT([xf86-input-egalax],
        0.1,
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        xf86-input-egalax)

AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_AUX_DIR(.)
AM_INIT_AUTOMAKE([dist-bzip2])

AM_MAINTAINER_MODE
m4_ifndef([XORG_MACROS_VERSION], [AC_FATAL([must install xorg-macros 1.2 or late
r before running autoconf/autogen])])
XORG_MACROS_VERSION(1.2)
AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CC

AH_TOP([#include "xorg-server.h"])

AC_ARG_WITH(xorg-module-dir,
            AC_HELP_STRING([--with-xorg-module-dir=DIR],
                           [Default xorg module directory [[default=$libdir/xorg
/modules]]]),
            [moduledir="$withval"],
            [moduledir="$libdir/xorg/modules"])
inputdir=${moduledir}/input
AC_SUBST(inputdir)

# Checks for pkg-config packages. We need to be able to override sdkdir
# to satisfy silly distcheck requirements.
PKG_CHECK_MODULES(XORG, xorg-server xproto $REQUIRED_MODULES)
XORG_CFLAGS="$CWARNFLAGS $XORG_CFLAGS"
AC_ARG_WITH([sdkdir], [],
    [sdkdir="$withval"],
    [sdkdir=`$PKG_CONFIG --variable=sdkdir xorg-server`])
AC_SUBST([sdkdir])

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC

DRIVER_NAME=egalax
AC_SUBST([DRIVER_NAME])

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile src/Makefile man/Makefile])
