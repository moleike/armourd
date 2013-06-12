#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.50])
AC_INIT([armourd], [1.0], [alexmorenocano@gmail.com])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_ARG_WITH([dbus],
    AS_HELP_STRING([--without-dbus], [dont use dbus (default is yes, when available)]))

AS_IF([test "x$with_dbus" != "xno"],
      [PKG_CHECK_MODULES([DBUS], [dbus-1], 
      [have_dbus=yes], [have_dbus=no])
])

AS_IF([test "x$have_dbus" = "xyes"],
      [AC_DEFINE([HAVE_DBUS], [], [build with dbus interfaces])],
      [AS_IF([test "x$with_dbus" = "xyes"],
             [AC_MSG_ERROR([Dbus interface requested but missing required package])
      ])
])

AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)


# Checks for header files.
AC_CHECK_HEADERS([err.h sys/signalfd.h sys/epoll.h linux/netlink.h \
                  linux/connector.h linux/cn_proc.h linux/filter.h \
                  sys/timerfd.h sys/eventfd.h ])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_UID_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([dup2 memset pathconf socket strdup strndup strtol])

AM_CONDITIONAL([DBUS], [test "x$with_dbus" != "xno" -a "x$have_dbus" = "xyes"])
AC_CONFIG_FILES([Makefile armourd.conf])
AC_OUTPUT
