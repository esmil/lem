/*
 * This file is part of LEM, a Lua Event Machine.
 * Copyright 2011-2012 Emil Renner Berthing
 *
 * LEM is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * LEM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LEM.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>

#if defined(__FreeBSD__) || defined(__APPLE__)
#include <netinet/in.h>
#else
#include <sys/sendfile.h>
#endif

#include <streams.h>

#include "sendfile.c"
#include "file.c"
#include "stream.c"
#include "server.c"
#include "tcp.c"
#include "parsers.c"

static int
module_index(lua_State *T)
{
	const char *key = lua_tostring(T, 2);
	int fd;

	if (strcmp(key, "stdin") == 0)
		fd = 0;
	else if (strcmp(key, "stdout") == 0)
		fd = 1;
	else if (strcmp(key, "stderr") == 0)
		fd = 2;
	else
		return 0;

	/* make the socket non-blocking */
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		lua_pushnil(T);
		lua_pushfstring(T, "error making filedescriptor non-blocking: %s",
		                strerror(errno));
		return 2;
	}

	if (fd == 0)
		(void)istream_new(T, fd, lua_upvalueindex(1));
	else
		(void)ostream_new(T, fd, lua_upvalueindex(2));

	/* save this object so we don't initialize it again */
	lua_pushvalue(T, 2);
	lua_pushvalue(T, -2);
	lua_rawset(T, 1);

	return 1;
}

int
luaopen_lem_streams_core(lua_State *L)
{
	/* create module table */
	lua_newtable(L);

	/* create metatable for sendfile objects */
	lua_newtable(L);
	/* mt.__index = mt */
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	/* mt.__gc = <sendfile_gc> */
	lua_pushcfunction(L, sendfile_gc);
	lua_setfield(L, -2, "__gc");
	/* mt.close = <sendfile_close> */
	lua_pushcfunction(L, sendfile_close);
	lua_setfield(L, -2, "close");
	/* mt.size = <sendfile_size> */
	lua_pushcfunction(L, sendfile_size);
	lua_setfield(L, -2, "size");
	/* insert table */
	lua_setfield(L, -2, "SendFile");

	/* insert sendfile function */
	lua_getfield(L, -1, "SendFile"); /* upvalue 1 = SendFile */
	lua_pushcclosure(L, sendfile_open, 1);
	lua_setfield(L, -2, "sendfile");

	/* create metatable for input stream objects */
	lua_newtable(L);
	/* mt.__index = mt */
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	/* mt.__gc = <stream_close> */
	lua_pushcfunction(L, istream_close);
	lua_setfield(L, -2, "__gc");
	/* mt.closed = <stream_closed> */
	lua_pushcfunction(L, stream_closed);
	lua_setfield(L, -2, "closed");
	/* mt.busy = <stream_busy> */
	lua_pushcfunction(L, stream_busy);
	lua_setfield(L, -2, "busy");
	/* mt.interrupt = <stream_interrupt> */
	lua_pushcfunction(L, stream_interrupt);
	lua_setfield(L, -2, "interrupt");
	/* mt.close = <istream_close> */
	lua_pushcfunction(L, istream_close);
	lua_setfield(L, -2, "close");
	/* mt.readp = <stream_readp> */
	lua_pushcfunction(L, stream_readp);
	lua_setfield(L, -2, "readp");
	/* insert table */
	lua_setfield(L, -2, "IStream");

	/* create metatable for output stream objects */
	lua_newtable(L);
	/* mt.__index = mt */
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	/* mt.__gc = <ostream_close> */
	lua_pushcfunction(L, ostream_close);
	lua_setfield(L, -2, "__gc");
	/* mt.closed = <stream_closed> */
	lua_pushcfunction(L, stream_closed);
	lua_setfield(L, -2, "closed");
	/* mt.busy = <stream_busy> */
	lua_pushcfunction(L, stream_busy);
	lua_setfield(L, -2, "busy");
	/* mt.interrupt = <stream_interrupt> */
	lua_pushcfunction(L, stream_interrupt);
	lua_setfield(L, -2, "interrupt");
	/* mt.close = <ostream_close> */
	lua_pushcfunction(L, ostream_close);
	lua_setfield(L, -2, "close");
	/* mt.write = <stream_write> */
	lua_pushcfunction(L, stream_write);
	lua_setfield(L, -2, "write");
	/* mt.cork = <stream_cork> */
	lua_pushcfunction(L, stream_cork);
	lua_setfield(L, -2, "cork");
	/* mt.uncork = <stream_uncork> */
	lua_pushcfunction(L, stream_uncork);
	lua_setfield(L, -2, "uncork");
	/* mt.sendfile = <ostream_sendfile> */
	lua_pushcfunction(L, stream_sendfile);
	lua_setfield(L, -2, "sendfile");
	/* insert table */
	lua_setfield(L, -2, "OStream");

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
	/* insert table */
	lua_setfield(L, -2, "File");

	/* create metatable for server objects */
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
	lua_getfield(L, -2, "IStream"); /* upvalue 1 = IStream */
	lua_getfield(L, -3, "OStream"); /* upvalue 2 = OStream */
	lua_pushcclosure(L, server_accept, 2);
	lua_setfield(L, -2, "accept");
	/* mt.autospawn = <server_autospawn> */
	lua_getfield(L, -2, "IStream"); /* upvalue 1 = IStream */
	lua_getfield(L, -3, "OStream"); /* upvalue 2 = OStream */
	lua_pushcclosure(L, server_autospawn, 2);
	lua_setfield(L, -2, "autospawn");
	/* insert table */
	lua_setfield(L, -2, "Server");

	/* insert open function */
	lua_getfield(L, -1, "IStream"); /* upvalue 1 = IStream */
	lua_getfield(L, -2, "OStream"); /* upvalue 2 = OStream */
	lua_getfield(L, -3, "File");    /* upvalue 3 = File */
	lua_pushcclosure(L, stream_open, 3);
	lua_setfield(L, -2, "open");
	/* insert popen function */
	lua_getfield(L, -1, "IStream"); /* upvalue 1 = IStream */
	lua_getfield(L, -2, "OStream"); /* upvalue 2 = OStream */
	lua_pushcclosure(L, stream_popen, 2);
	lua_setfield(L, -2, "popen");

	/* insert the connect function */
	lua_getfield(L, -1, "IStream"); /* upvalue 1 = IStream */
	lua_getfield(L, -2, "OStream"); /* upvalue 2 = OStream */
	lua_pushcclosure(L, tcp_connect, 2);
	lua_setfield(L, -2, "tcp_connect");
	/* insert the tcp4_listen function */
	lua_getfield(L, -1, "Server"); /* upvalue 1 = Server */
	lua_pushcclosure(L, tcp4_listen, 1);
	lua_setfield(L, -2, "tcp4_listen");
	/* insert the tcp6_listen function */
	lua_getfield(L, -1, "Server"); /* upvalue 1 = Server */
	lua_pushcclosure(L, tcp6_listen, 1);
	lua_setfield(L, -2, "tcp6_listen");

	/* create parser table */
	lua_createtable(L, 0, 4);
	/* push parser_line */
	lua_pushlightuserdata(L, (void *)&parser_available);
	lua_setfield(L, -2, "available");
	/* push parser_target */
	lua_pushlightuserdata(L, (void *)&parser_target);
	lua_setfield(L, -2, "target");
	/* push parser_all */
	lua_pushlightuserdata(L, (void *)&parser_all);
	lua_setfield(L, -2, "*a");
	/* push parser_line */
	lua_pushlightuserdata(L, (void *)&parser_line);
	lua_setfield(L, -2, "*l");
	/* insert parser table */
	lua_setfield(L, -2, "parsers");

	/* create metatable for the module */
	lua_newtable(L);
	/* insert the index function */
	lua_getfield(L, -2, "IStream"); /* upvalue 1 = IStream */
	lua_getfield(L, -3, "OStream"); /* upvalue 2 = OStream */
	lua_pushcclosure(L, module_index, 2);
	lua_setfield(L, -2, "__index");

	/* set the metatable */
	lua_setmetatable(L, -2);

	return 1;
}
