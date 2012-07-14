AC_PREREQ([2.69])
AC_INIT([[lem]], [[0.3]], [[esmil@mailme.dk]])
builtin_lua_version='5.2'

AC_LANG(C)
AC_CONFIG_HEADERS([libev/ev-config.h:ev-config.h.in])

AC_SUBST([headers], ['lem.h'])
AC_SUBST([objects], ['lem.o'])
AC_SUBST([SHARED])

AC_ARG_WITH([debug],
  [AS_HELP_STRING([--with-debug],
    [enable debugging output @<:@default=no@:>@])],
  [],
  [CPPFLAGS="$CPPFLAGS -DNDEBUG"])

AC_ARG_WITH([lua],
  [AS_HELP_STRING([--with-lua],
    [check, builtin, lua or luajit @<:@default=check@:>@])],
  [AS_CASE(["$with_lua"], [check],,[builtin],,[lua],,[luajit],,
    [AC_MSG_ERROR([invalid argument --with-lua=$with_lua])])],
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
  [STRIP="$STRIP -x"],
  [SHARED='-shared'])

# Checks for Lua.
AS_CASE(["x$with_lua"],
  [xcheck],
  [PKG_CHECK_MODULES([lua], [lua],
    [with_lua=lua]
    [CPPFLAGS="$CPPFLAGS $lua_CFLAGS"]
    [LIBS="$lua_LIBS $LIBS"]
    [AS_IF([test "x$lmoddir" = 'x'], [lmoddir="`$PKG_CONFIG --variable=INSTALL_LMOD lua`"])]
    [AS_IF([test "x$cmoddir" = 'x'], [cmoddir="`$PKG_CONFIG --variable=INSTALL_CMOD lua`"])],
    [AC_CHECK_LIB([lua], [lua_newstate],
      [AC_CHECK_HEADER([lua.h],
	[with_lua=lua]
        [LIBS="-llua $LIBS"],
        [with_lua=builtin])],
      [with_lua=builtin])])],
  [xlua],
  [PKG_CHECK_MODULES([lua], [lua],
    [CPPFLAGS="$CPPFLAGS $lua_CFLAGS"]
    [LIBS="$lua_LIBS $LIBS"]
    [AS_IF([test "x$lmoddir" = 'x'], [lmoddir="`$PKG_CONFIG --variable=INSTALL_LMOD lua`"])]
    [AS_IF([test "x$cmoddir" = 'x'], [cmoddir="`$PKG_CONFIG --variable=INSTALL_CMOD lua`"])],
    [AC_CHECK_LIB([lua], [lua_newstate],
      [],
      [AC_MSG_ERROR([Lua library requested, but no Lua library found])])]
    [AC_CHECK_HEADER([lua.h],
      [],
      [AC_MSG_ERROR([Lua library requested, but no lua.h header found])])])],
  [xluajit],
  [PKG_CHECK_MODULES([luajit], [luajit],
    [CPPFLAGS="$CPPFLAGS $luajit_CFLAGS"]
    [LIBS="$luajit_LIBS $LIBS"]
    [AS_IF([test "x$lmoddir" = 'x'], [lmoddir="`$PKG_CONFIG --variable=INSTALL_LMOD luajit`"])]
    [AS_IF([test "x$cmoddir" = 'x'], [cmoddir="`$PKG_CONFIG --variable=INSTALL_CMOD luajit`"])],
    [AC_MSG_ERROR([LuaJIT library requested, but no pkg-config file found])])])

AS_IF([test "x$with_lua" = 'xbuiltin'],
  [AC_CHECK_LIB([m], [sin])]
  [AC_SEARCH_LIBS([dlopen], [dl])]
  [objects="lua/amalg.o $objects"]
  [headers="lua/luaconf.h lua/lua.h lua/lauxlib.h $headers"]
  [CPPFLAGS="$CPPFLAGS -Ilua"]
  [AS_IF([test "x$lmoddir" = 'x'], [lmoddir="\${datarootdir}/lua/$builtin_lua_version"])]
  [AS_IF([test "x$cmoddir" = 'x'], [cmoddir="\${libdir}/lua/$builtin_lua_version"])])

# Checks for header files.
AC_CHECK_HEADERS([stddef.h stdlib.h string.h unistd.h time.h])
AC_CHECK_HEADERS([sys/eventfd.h sys/epoll.h sys/event.h])

#  [AC_RUN_IFELSE([AC_LANG_PROGRAM([
##include <stdio.h>
##include <lua.h>
#], [
#FILE *f = fopen("conftest.out", "w");
#fputs(LUA_VERSION, f);
#])],
#    [echo "conftest returned: `cat conftest.out`"],[])])

objects="libev.o $objects"
headers="libev/ev-config.h libev/ev.h $headers"
CPPFLAGS="$CPPFLAGS -Ilibev"

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

AS_IF([test "x$lmoddir" = 'x'],
  [AC_MSG_ERROR([unable to deduce Lua module directory, please use --with-lmoddir=path])])
AS_IF([test "x$cmoddir" = 'x'],
  [AC_MSG_ERROR([unable to deduce Lua C module directory, please use --with-cmoddir=path])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
