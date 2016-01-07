AC_PREREQ([2.69])
AC_INIT([[lem]], [[0.3]], [[esmil@mailme.dk]])
builtin_lua_version='5.3'

AC_LANG(C)
AC_CONFIG_HEADERS([libev/ev-config.h:ev-config.h.in])

AC_SUBST([headers], ['lem.h lem-parsers.h'])
AC_SUBST([objects], ['bin/lem.o'])
AC_SUBST([CPPFLAGS_ADD], ['-Iinclude'])
AC_SUBST([SHARED], ['-shared'])

AS_IF([test "x$CFLAGS" = 'x'], CFLAGS='-O2 -g -Wall -Wextra')

AC_ARG_WITH([lua],
  [AS_HELP_STRING([--with-lua],
    [check, builtin or pkg-config name @<:@default=check@:>@])],
  [],
  [with_lua=check])

AC_SUBST([lmoddir])
AC_ARG_WITH([lmoddir],
  [AS_HELP_STRING([--with-lmoddir],
    [Lua module installation directory])],
  [lmoddir="$with_lmoddir"])

AC_SUBST([cmoddir])
AC_ARG_WITH([cmoddir],
  [AS_HELP_STRING([--with-cmoddir],
    [Lua C module installation directory])],
  [cmoddir="$with_cmoddir"])

PKG_INSTALLDIR
AC_CANONICAL_TARGET

# Checks for programs.
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_SED
AC_PATH_TARGET_TOOL([STRIP],[strip])
PKG_PROG_PKG_CONFIG

# Target specific fixes
AS_CASE(["x$target_os"],
  [xdarwin*],
  [ac_cv_func_kqueue=no] # kqueue seems to be broken on OSX
  [SHARED='-dynamiclib -Wl,-undefined,dynamic_lookup']
  [STRIP="$STRIP -x"])

# Check for pthread library
AC_SEARCH_LIBS([pthread_create], [pthread])

# Configure libev
objects="bin/libev.o $objects"
headers="ev-config.h ev.h $headers"
CPPFLAGS_ADD="$CPPFLAGS_ADD -Ilibev"

# Configure Lua
AS_CASE(["x$with_lua"],
  [xbuiltin],
  [],
  [xcheck],
  [PKG_CHECK_MODULES([Lua], [lua5.3], [with_lua=lua5.3],
  [PKG_CHECK_MODULES([Lua], [lua53],  [with_lua=lua53],
  [PKG_CHECK_MODULES([Lua], [lua],
    [AS_IF([test "x$($PKG_CONFIG --variable=V lua)" = 'x5.3'],
      [with_lua=lua],
      [with_lua=builtin])],
    [with_lua=builtin])])])],
  [PKG_CHECK_MODULES([Lua], ["$with_lua"], [],
    [AC_MSG_ERROR(["pkg-config package '$with_lua' not found"])])])

AS_IF([test "x$with_lua" = 'xbuiltin'],
  [AC_SEARCH_LIBS([sin], [m])]
  [AC_SEARCH_LIBS([dlopen], [dl])]
  [objects="bin/lua.o $objects"]
  [headers="luaconf.h lua.h lauxlib.h $headers"]
  [CPPFLAGS_ADD="$CPPFLAGS_ADD -Ilua"]
  [Lua_CFLAGS='']
  [Lua_LIBS='']
  [ac_cv_func_luaL_traceback=yes]
  [AS_IF([test "x$lmoddir" = 'x'], [lmoddir="\${datarootdir}/lua/$builtin_lua_version"])]
  [AS_IF([test "x$cmoddir" = 'x'], [cmoddir="\${libdir}/lua/$builtin_lua_version"])])

CPPFLAGS_ADD="$CPPFLAGS_ADD $Lua_CFLAGS"
LIBS="$Lua_LIBS $LIBS"
AS_IF([test "x$lmoddir" = 'x'], [lmoddir="`$PKG_CONFIG --variable=INSTALL_LMOD $with_lua`"])
AS_IF([test "x$cmoddir" = 'x'], [cmoddir="`$PKG_CONFIG --variable=INSTALL_CMOD $with_lua`"])

AS_IF([test "x$lmoddir" = 'x'],
  [AC_MSG_ERROR([unable to deduce Lua module directory, please use --with-lmoddir=path])])
AS_IF([test "x$cmoddir" = 'x'],
  [AC_MSG_ERROR([unable to deduce Lua C module directory, please use --with-cmoddir=path])])

# Checks for header files.
AC_CHECK_HEADERS([stddef.h stdlib.h string.h unistd.h sys/time.h time.h pthread.h])
AC_CHECK_HEADERS([sys/eventfd.h sys/epoll.h sys/event.h])

AS_IF([test "x$ac_cv_header_sys_eventfd_h" = 'xyes'],
  [AC_CHECK_FUNC([eventfd],
    [AC_DEFINE([EV_USE_EVENTFD], 1)],
    [AC_DEFINE([EV_USE_EVENTFD], 0)])],
  [AC_DEFINE([EV_USE_EVENTFD], 0)])

AC_CHECK_FUNC([nanosleep],
  [AC_DEFINE([EV_USE_NANOSLEEP], 1)],
  [AC_DEFINE([EV_USE_NANOSLEEP], 0)])

AC_CHECK_FUNCS([poll select epoll_ctl kqueue])

AS_IF([test "x$ac_cv_func_epoll_ctl" = 'xyes'], [
  AC_DEFINE([EV_USE_SELECT], 0)
  AC_DEFINE([EV_USE_POLL], 0)
  AC_DEFINE([EV_USE_EPOLL], 1)
  AC_DEFINE([EV_USE_KQUEUE], 0)
], [AS_IF([test "x$ac_cv_func_kqueue" = 'xyes'], [
  AC_DEFINE([EV_USE_SELECT], 0)
  AC_DEFINE([EV_USE_POLL], 0)
  AC_DEFINE([EV_USE_EPOLL], 0)
  AC_DEFINE([EV_USE_KQUEUE], 1)
], [AS_IF([test "x$ac_cv_func_select" = 'xyes'], [
  AC_DEFINE([EV_USE_SELECT], 1)
  AC_DEFINE([EV_USE_POLL], 0)
  AC_DEFINE([EV_USE_EPOLL], 0)
  AC_DEFINE([EV_USE_KQUEUE], 0)
], [AS_IF([test "x$ac_cv_func_poll" = 'xyes'], [
  AC_DEFINE([EV_USE_SELECT], 0)
  AC_DEFINE([EV_USE_POLL], 1)
  AC_DEFINE([EV_USE_EPOLL], 0)
  AC_DEFINE([EV_USE_KQUEUE], 0)
], [
  AC_MSG_ERROR([neither poll or select found])
])])])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
