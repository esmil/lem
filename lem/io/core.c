/*
 * This file is part of LEM, a Lua Event Machine.
 * Copyright 2011-2013 Emil Renner Berthing
 *
 * LEM is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * LEM is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with LEM.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>

#if defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/un.h>
#include <netinet/in.h>
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 104
#endif
#else
#include <linux/un.h>
#include <sys/sendfile.h>
#endif

#include <lem-parsers.h>

static int
io_closed(lua_State *T)
{
	lua_pushnil(T);
	lua_pushliteral(T, "closed");
	return 2;
}

static int
io_busy(lua_State *T)
{
	lua_pushnil(T);
	lua_pushliteral(T, "busy");
	return 2;
}

static int
io_strerror(lua_State *T, int err)
{
	lua_pushnil(T);
	lua_pushstring(T, strerror(err));
	return 2;
}

static int
io_optperm(lua_State *T, int idx)
{
	lua_Number n = luaL_optnumber(T, idx, -1);
	int mode = n;
	int octal;
	int i;

	if ((lua_Number)mode != n)
		goto error;
	if (mode == -1)
		return -1;
	if (mode < 0)
		goto error;

	octal = 0;
	for (i = 1; i <= 64; i *= 8) {
		int digit = mode % 10;
		if (digit > 7)
			goto error;

		octal += digit * i;
		mode /= 10;
	}
	if (mode != 0)
		goto error;

	return octal;
error:
	return luaL_argerror(T, idx, "invalid permissions");
}

#include "file.c"
#include "stream.c"
#include "server.c"
#include "tcp.c"
#include "unix.c"

struct open {
	struct lem_async a;
	const char *path;
	int fd;
	int flags;
};

static void
io_open_work(struct lem_async *a)
{
	struct open *o = (struct open *)a;
	int fd;
	struct stat st;

	fd = open(o->path, o->flags | O_NONBLOCK,
			o->fd >= 0 ? o->fd :
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd < 0) {
		o->flags = -errno;
		return;
	}

	if (fstat(fd, &st)) {
		o->flags = -errno;
		close(fd);
		return;
	}

	o->fd = fd;
	lem_debug("st.st_mode & S_IFMT = %o", st.st_mode & S_IFMT);
	switch (st.st_mode & S_IFMT) {
	case S_IFREG:
	case S_IFBLK:
		o->flags = 0;
		break;

	case S_IFCHR:
	case S_IFIFO:
		o->flags = 1;
		break;

	default:
		o->flags = -EINVAL;
		break;
	}
}

static void
io_open_reap(struct lem_async *a)
{
	struct open *o = (struct open *)a;
	lua_State *T = o->a.T;
	int fd = o->fd;
	int ret = o->flags;

	lem_debug("ret = %d", ret);
	free(o);

	switch (ret) {
	case 0: file_new(T, fd, 2); break;
	case 1: stream_new(T, fd, 3); break;
	default:
		lem_queue(T, io_strerror(T, -ret));
		return;
	}

	lem_queue(T, 1);
}

static int
io_mode_to_flags(const char *mode)
{
	int omode;
	int oflags;

	switch (*mode++) {
	case 'r':
		omode = O_RDONLY;
		oflags = 0;
		break;
	case 'w':
		omode = O_WRONLY;
		oflags = O_CREAT | O_TRUNC;
		break;
	case 'a':
		omode = O_WRONLY;
		oflags = O_CREAT | O_APPEND;
		break;
	default:
		return -1;
	}

next:
	switch (*mode++) {
	case '\0':
		break;
	case '+':
		omode = O_RDWR;
		goto next;
	case 'b':
		/* this does nothing on *nix, but
		 * don't treat it as an error */
		goto next;
	case 'x':
		oflags |= O_EXCL;
		goto next;
	default:
		return -1;
	}

	return omode | oflags;
}

static int
io_open(lua_State *T)
{
	const char *path = luaL_checkstring(T, 1);
	int flags = io_mode_to_flags(luaL_optstring(T, 2, "r"));
	int perm = io_optperm(T, 3);
	struct open *o;

	if (flags < 0)
		return luaL_error(T, "invalid mode string");

	o = lem_xmalloc(sizeof(struct open));
	o->path = path;
	o->fd = perm;
	o->flags = flags;
	lem_async_do(&o->a, T, io_open_work, io_open_reap);

	lua_settop(T, 1);
	lua_pushvalue(T, lua_upvalueindex(1));
	lua_pushvalue(T, lua_upvalueindex(2));
	return lua_yield(T, 3);
}

static void
push_stdstream(lua_State *L, int fd)
{
	struct stream *s;

	/* make the socket non-blocking */
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
		luaL_error(L, "error making fd %d non-blocking: %s",
				fd, strerror(errno));

	s = stream_new(L, fd, -2);
	s->open = 2; /* don't close this in __gc() */
}

int
luaopen_lem_io_core(lua_State *L)
{
	/* create module table */
	lua_newtable(L);

	/* create File metatable */
	lua_newtable(L);
	/* mt.__index = mt */
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	/* mt.__gc = <file_gc> */
	lua_pushcfunction(L, file_gc);
	lua_setfield(L, -2, "__gc");
	/* mt.closed = <file_closed> */
	lua_pushcfunction(L, file_closed);
	lua_setfield(L, -2, "closed");
	/* mt.close = <file_close> */
	lua_pushcfunction(L, file_close);
	lua_setfield(L, -2, "close");
	/* mt.readp = <file_readp> */
	lua_pushcfunction(L, file_readp);
	lua_setfield(L, -2, "readp");
	/* mt.write = <file_write> */
	lua_pushcfunction(L, file_write);
	lua_setfield(L, -2, "write");
	/* mt.size = <file_size> */
	lua_pushcfunction(L, file_size);
	lua_setfield(L, -2, "size");
	/* mt.seek = <file_seek> */
	lua_pushcfunction(L, file_seek);
	lua_setfield(L, -2, "seek");
	/* mt.lock = <file_lock> */
	lua_pushcfunction(L, file_lock);
	lua_setfield(L, -2, "lock");
	/* insert table */
	lua_setfield(L, -2, "File");

	/* create Stream metatable */
	lua_newtable(L);
	/* mt.__index = mt */
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	/* mt.__gc = <stream_gc> */
	lua_pushcfunction(L, stream_gc);
	lua_setfield(L, -2, "__gc");
	/* mt.closed = <stream_closed> */
	lua_pushcfunction(L, stream_closed);
	lua_setfield(L, -2, "closed");
	/* mt.close = <stream_close> */
	lua_pushcfunction(L, stream_close);
	lua_setfield(L, -2, "close");
	/* mt.readp = <stream_readp> */
	lua_pushcfunction(L, stream_readp);
	lua_setfield(L, -2, "readp");
	/* mt.write = <stream_write> */
	lua_pushcfunction(L, stream_write);
	lua_setfield(L, -2, "write");
	/* mt.cork = <stream_cork> */
	lua_pushcfunction(L, stream_cork);
	lua_setfield(L, -2, "cork");
	/* mt.uncork = <stream_uncork> */
	lua_pushcfunction(L, stream_uncork);
	lua_setfield(L, -2, "uncork");
	/* mt.sendfile = <stream_sendfile> */
	lua_pushcfunction(L, stream_sendfile);
	lua_setfield(L, -2, "sendfile");
	/* insert io.stdin stream */
	push_stdstream(L, STDIN_FILENO);
	lua_setfield(L, -3, "stdin");
	/* insert io.stdout stream */
	push_stdstream(L, STDOUT_FILENO);
	lua_setfield(L, -3, "stdout");
	/* insert io.stderr stream */
	push_stdstream(L, STDERR_FILENO);
	lua_setfield(L, -3, "stderr");
	/* insert table */
	lua_setfield(L, -2, "Stream");

	/* create Server metatable */
	lua_newtable(L);
	/* mt.__index = mt */
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	/* mt.__gc = <server_close> */
	lua_pushcfunction(L, server_close);
	lua_setfield(L, -2, "__gc");
	/* mt.closed = <server_closed> */
	lua_pushcfunction(L, server_closed);
	lua_setfield(L, -2, "closed");
	/* mt.busy = <server_busy> */
	lua_pushcfunction(L, server_busy);
	lua_setfield(L, -2, "busy");
	/* mt.close = <server_close> */
	lua_pushcfunction(L, server_close);
	lua_setfield(L, -2, "close");
	/* mt.interrupt = <server_interrupt> */
	lua_pushcfunction(L, server_interrupt);
	lua_setfield(L, -2, "interrupt");
	/* mt.accept = <server_accept> */
	lua_getfield(L, -2, "Stream"); /* upvalue 1 = Stream */
	lua_pushcclosure(L, server_accept, 1);
	lua_setfield(L, -2, "accept");
	/* mt.autospawn = <server_autospawn> */
	lua_getfield(L, -2, "Stream"); /* upvalue 1 = Stream */
	lua_pushcclosure(L, server_autospawn, 1);
	lua_setfield(L, -2, "autospawn");
	/* insert table */
	lua_setfield(L, -2, "Server");

	/* insert open function */
	lua_getfield(L, -1, "File");   /* upvalue 1 = File */
	lua_getfield(L, -2, "Stream"); /* upvalue 2 = Stream */
	lua_pushcclosure(L, io_open, 2);
	lua_setfield(L, -2, "open");
	/* insert popen function */
	lua_getfield(L, -1, "Stream"); /* upvalue 1 = Stream */
	lua_pushcclosure(L, stream_popen, 1);
	lua_setfield(L, -2, "popen");

	/* create tcp table */
	lua_createtable(L, 0, 0);
	/* insert the connect function */
	lua_getfield(L, -2, "Stream"); /* upvalue 1 = Stream */
	lua_pushcclosure(L, tcp_connect, 1);
	lua_setfield(L, -2, "connect");
	/* insert the listen4 function */
	lua_getfield(L, -2, "Server"); /* upvalue 1 = Server */
	lua_pushcclosure(L, tcp_listen4, 1);
	lua_setfield(L, -2, "listen4");
	/* insert the listen6 function */
	lua_getfield(L, -2, "Server"); /* upvalue 1 = Server */
	lua_pushcclosure(L, tcp_listen6, 1);
	lua_setfield(L, -2, "listen6");
	/* insert the tcp table */
	lua_setfield(L, -2, "tcp");

	/* create unix table */
	lua_createtable(L, 0, 0);
	/* insert the connect function */
	lua_getfield(L, -2, "Stream"); /* upvalue 1 = Stream */
	lua_pushcclosure(L, unix_connect, 1);
	lua_setfield(L, -2, "connect");
	/* insert the listen function */
	lua_getfield(L, -2, "Server"); /* upvalue 1 = Server */
	lua_pushcclosure(L, unix_listen, 1);
	lua_setfield(L, -2, "listen");
	/* insert the tcp table */
	lua_setfield(L, -2, "unix");

	return 1;
}
