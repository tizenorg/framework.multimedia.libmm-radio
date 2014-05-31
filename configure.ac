#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(libmm-radio, 0.1.0, heechul.jeon@samsung.com)
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([src/mm_radio.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_LIBTOOL
AC_PROG_CC
AM_PROG_CC_C_O

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h sys/ioctl.h sys/time.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_PID_T
AC_HEADER_TIME

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strstr])

PKG_CHECK_MODULES(GTHREAD, gthread-2.0)
AC_SUBST(GTHREAD_CFLAGS)
AC_SUBST(GTHREAD_LIBS)

PKG_CHECK_MODULES(MMCOMMON, mm-common)
AC_SUBST(MMCOMMON_CFLAGS)
AC_SUBST(MMCOMMON_LIBS)

PKG_CHECK_MODULES(MMTA, mm-ta)
AC_SUBST(MMTA_CFLAGS)
AC_SUBST(MMTA_LIBS)

PKG_CHECK_MODULES(GST, gstreamer-0.10)
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

PKG_CHECK_MODULES(GSTAPP, gstreamer-app-0.10)
AC_SUBST(GSTAPP_CFLAGS)
AC_SUBST(GSTAPP_LIBS)

PKG_CHECK_MODULES(MMSESSION, mm-session)
AC_SUBST(MMSESSION_CFLAGS)
AC_SUBST(MMSESSION_LIBS)

PKG_CHECK_MODULES(AUDIOSESSIONMGR, audio-session-mgr)
AC_SUBST(AUDIOSESSIONMGR_CFLAGS)
AC_SUBST(AUDIOSESSIONMGR_LIBS)

PKG_CHECK_MODULES(MMSOUND, mm-sound)
AC_SUBST(MMSOUND_CFLAGS)
AC_SUBST(MMSOUND_LIBS)

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	test/Makefile
	mm-radio.pc
])
AC_OUTPUT
