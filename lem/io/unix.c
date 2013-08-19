/*
 * This file is part of LEM, a Lua Event Machine.
 * Copyright 2013 Emil Renner Berthing
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

struct unix_create {
	struct lem_async a;
	const char *path;
	size_t len;
	int sock;
	int err;
};

static void
unix_connect_work(struct lem_async *a)
{
	struct unix_create *u = (struct unix_create *)a;
	struct sockaddr_un addr;
	int sock;

	sock = socket(AF_UNIX,
#ifdef SOCK_CLOEXEC
			SOCK_CLOEXEC |
#endif
			SOCK_STREAM, 0);
	if (sock < 0) {
		u->sock = -1;
		u->err = errno;
		return;
	}
#ifndef SOCK_CLOEXEC
	if (fcntl(sock, F_SETFD, FD_CLOEXEC) == -1) {
		u->sock = -1;
		u->err = errno;
		goto error;
	}
#endif
	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, u->path, u->len+1);

	if (connect(sock, (struct sockaddr *)&addr,
				offsetof(struct sockaddr_un, sun_path) + u->len)) {
		u->sock = -2;
		u->err = errno;
		goto error;
	}

	/* make the socket non-blocking */
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		u->sock = -1;
		u->err = errno;
		goto error;
	}

	u->sock = sock;
	return;

error:
	close(sock);
}

static void
unix_connect_reap(struct lem_async *a)
{
	struct unix_create *u = (struct unix_create *)a;
	lua_State *T = u->a.T;
	int sock = u->sock;

	if (sock >= 0) {
		free(u);

		stream_new(T, sock, 2);
		lem_queue(T, 1);
		return;
	}

	lua_pushnil(T);
	switch (-sock) {
	case 1:
		lua_pushfstring(T, "error creating socket: %s",
				strerror(u->err));
		break;
	case 2:
		lua_pushfstring(T, "error connecting to '%s': %s",
				u->path, strerror(u->err));
		break;
	}
	lem_queue(T, 2);
	free(u);
}

static int
unix_connect(lua_State *T)
{
	size_t len;
	const char *path = luaL_checklstring(T, 1, &len);
	struct unix_create *u;

	if (len >= UNIX_PATH_MAX)
		return luaL_argerror(T, 1, "path too long");

	u = lem_xmalloc(sizeof(struct unix_create));
	u->path = path;
	u->len = len;
	lem_async_do(&u->a, T, unix_connect_work, unix_connect_reap);

	lua_settop(T, 1);
	lua_pushvalue(T, lua_upvalueindex(1));
	return lua_yield(T, 2);
}

static void
unix_listen_work(struct lem_async *a)
{
	struct unix_create *u = (struct unix_create *)a;
	struct sockaddr_un addr;
	int sock;

	sock = socket(AF_UNIX,
#ifdef SOCK_CLOEXEC
			SOCK_CLOEXEC |
#endif
			SOCK_STREAM, 0);
	if (sock < 0) {
		u->sock = -1;
		u->err = errno;
		return;
	}
#ifndef SOCK_CLOEXEC
	if (fcntl(sock, F_SETFD, FD_CLOEXEC) == -1) {
		u->sock = -1;
		u->err = errno;
		goto error;
	}
#endif
	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, u->path, u->len+1);

	if (bind(sock, (struct sockaddr *)&addr,
				offsetof(struct sockaddr_un, sun_path) + u->len)) {
		u->sock = -2;
		u->err = errno;
		goto error;
	}

	if (u->sock >= 0 && chmod(u->path, u->sock)) {
		u->sock = -3;
		u->err = errno;
		goto error;
	}

	if (listen(sock, u->err)) {
		u->sock = -4;
		u->err = errno;
		goto error;
	}

	/* make the socket non-blocking */
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		u->sock = -1;
		u->err = errno;
		goto error;
	}

	u->sock = sock;
	return;

error:
	close(sock);
}

static void
unix_listen_reap(struct lem_async *a)
{
	struct unix_create *u = (struct unix_create *)a;
	lua_State *T = u->a.T;
	int sock = u->sock;

	if (sock >= 0) {
		free(u);
		server_new(T, sock, 2);
		lem_queue(T, 1);
		return;
	}

	lua_pushnil(T);
	switch (-sock) {
	case 1:
		lua_pushfstring(T, "error creating socket: %s",
				strerror(u->err));
		break;
	case 2:
		lua_pushfstring(T, "error binding to '%s': %s",
				u->path, strerror(u->err));
		break;
	case 3:
		lua_pushfstring(T, "error setting permissions on '%s': %s",
				u->path, strerror(u->err));
		break;
	case 4:
		lua_pushfstring(T, "error listening on '%s': %s",
				u->path, strerror(u->err));
		break;
	}
	lem_queue(T, 2);
	free(u);
}

static int
unix_listen(lua_State *T)
{
	size_t len;
	const char *path = luaL_checklstring(T, 1, &len);
	int perm = io_optperm(T, 2);
	int backlog = (int)luaL_optnumber(T, 3, MAXPENDING);
	struct unix_create *u;

	if (len >= UNIX_PATH_MAX)
		return luaL_argerror(T, 1, "path too long");

	u = lem_xmalloc(sizeof(struct unix_create));
	u->path = path;
	u->len = len;
	u->sock = perm;
	u->err = backlog;
	lem_async_do(&u->a, T, unix_listen_work, unix_listen_reap);

	lua_settop(T, 1);
	lua_pushvalue(T, lua_upvalueindex(1));
	return lua_yield(T, 2);
}
