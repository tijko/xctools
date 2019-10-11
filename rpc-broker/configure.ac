AC_INIT([rpc-broker], [1.0], [bug-automake@gnu.org])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

AC_PROG_CC
AC_CONFIG_FILES([Makefile src/Makefile])

PKG_CHECK_MODULES([DBUS], [dbus-1])
PKG_CHECK_MODULES([LIBWEBSOCKETS], [libwebsockets])
PKG_CHECK_MODULES([JSON_C], [json-c])
PKG_CHECK_MODULES([LIBXML], [libxml-2.0])

PKG_CHECK_MODULES([LIBARGO], [libargo],
   [AC_DEFINE([HAVE_ARGO], [1], [Use argo])],
   [AC_MSG_WARN([libargo not found, building without it])])
PKG_CHECK_MODULES([XENSTORE], [xenstore],
   [AC_DEFINE([HAVE_XENSTORE], [1], [Use xenstore])],
   [AC_MSG_WARN([xenstore not found, building without it])])

AC_OUTPUT
