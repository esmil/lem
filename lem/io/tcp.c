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

struct tcp_getaddr {
	struct lem_async a;
	const char *node;
	const char *service;
	int sock;
	int err;
};

static const int tcp_famnumber[] = { AF_UNSPEC, AF_INET, AF_INET6 };
static const char *const tcp_famnames[] = { "any", "ipv4", "ipv6", NULL };

static int
tcp_connect_try(struct tcp_getaddr *g, struct addrinfo *addr)
{
	int sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

	lem_debug("addr->ai_family = %d, sock = %d", addr->ai_family, sock);
	if (sock < 0) {
		int err = errno;

		if (err == EAFNOSUPPORT || err == EPROTONOSUPPORT)
			return 0;

		g->sock = -2;
		g->err = err;
		return 1;
	}

	/* connect */
	if (connect(sock, addr->ai_addr, addr->ai_addrlen)) {
		close(sock);
		return 0;
	}

	/* make the socket non-blocking */
	if (fcntl(sock, F_SETFL, O_NONBLOCK)) {
		g->sock = -3;
		g->err = errno;
		close(sock);
		return 1;
	}

	g->sock = sock;
	return 1;
}

static void
tcp_connect_work(struct lem_async *a)
{
	struct tcp_getaddr *g = (struct tcp_getaddr *)a;
	struct addrinfo hints = {
		.ai_flags     = 0,
		.ai_family    = tcp_famnumber[g->sock],
		.ai_socktype  = SOCK_STREAM,
		.ai_protocol  = IPPROTO_TCP,
		.ai_addrlen   = 0,
		.ai_addr      = NULL,
		.ai_canonname = NULL,
		.ai_next      = NULL
	};
	struct addrinfo *result;
	struct addrinfo *addr;
	int ret;

	/* lookup name */
	ret = getaddrinfo(g->node, g->service, &hints, &result);
	if (ret) {
		g->sock = -1;
		g->err = ret;
		return;
	}

	/* create the TCP socket */
	for (addr = result; addr; addr = addr->ai_next) {
		if (tcp_connect_try(g, addr))
			break;
	}

	freeaddrinfo(result);
	if (addr == NULL)
		g->sock = -4;
}

static void
tcp_connect_reap(struct lem_async *a)
{
	struct tcp_getaddr *g = (struct tcp_getaddr *)a;
	lua_State *T = g->a.T;
	const char *node = g->node;
	const char *service = g->service;
	int sock = g->sock;
	int err = g->err;

	free(g);

	lem_debug("connection established");
	if (sock >= 0) {
		stream_new(T, sock, 2);
		lem_queue(T, 1);
		return;
	}

	lua_pushnil(T);
	switch (-sock) {
	case 1:
		lua_pushfstring(T, "error looking up '%s:%s': %s",
				node, service, gai_strerror(err));
		break;
	case 2:
		lua_pushfstring(T, "error creating socket: %s",
				strerror(err));
		break;
	case 3:
		lua_pushfstring(T, "error making socket non-blocking: %s",
		                strerror(err));
		break;
	case 4:
		lua_pushfstring(T, "error connecting to '%s:%s'",
				node, service);
		break;
	}
	lem_queue(T, 2);
}

static int
tcp_connect(lua_State *T)
{
	const char *node = luaL_checkstring(T, 1);
	const char *service = luaL_checkstring(T, 2);
	int family = luaL_checkoption(T, 3, "any", tcp_famnames);
	struct tcp_getaddr *g;

	g = lem_xmalloc(sizeof(struct tcp_getaddr));
	g->node = node;
	g->service = service;
	g->sock = family;
	lem_async_do(&g->a, T, tcp_connect_work, tcp_connect_reap);

	lua_settop(T, 1);
	lua_pushvalue(T, lua_upvalueindex(1));
	return lua_yield(T, 2);
}

static void
tcp_listen_work(struct lem_async *a)
{
	struct tcp_getaddr *g = (struct tcp_getaddr *)a;
	struct addrinfo hints = {
		.ai_flags     = AI_PASSIVE,
		.ai_family    = tcp_famnumber[g->sock],
		.ai_socktype  = SOCK_STREAM,
		.ai_protocol  = IPPROTO_TCP,
		.ai_addrlen   = 0,
		.ai_addr      = NULL,
		.ai_canonname = NULL,
		.ai_next      = NULL
	};
	struct addrinfo *addr = NULL;
	int sock = -1;
	int ret;

	/* lookup name */
	ret = getaddrinfo(g->node, g->service, &hints, &addr);
	if (ret) {
		g->sock = -1;
		g->err = ret;
		goto error;
	}

	/* create the TCP socket */
	sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	lem_debug("addr->ai_family = %d, sock = %d", addr->ai_family, sock);
	if (sock < 0) {
		g->sock = -2;
		g->err = errno;
		goto error;
	}

	/* set SO_REUSEADDR option if possible */
	ret = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &ret, sizeof(int));

	/* bind */
	if (bind(sock, addr->ai_addr, addr->ai_addrlen)) {
		g->sock = -3;
		g->err = errno;
		goto error;
	}
	freeaddrinfo(addr);
	addr = NULL;

	/* listen to the socket */
	if (listen(sock, g->err)) {
		g->sock = -4;
		g->err = errno;
		goto error;
	}

	/* make the socket non-blocking */
	if (fcntl(sock, F_SETFL, O_NONBLOCK)) {
		g->sock = -5;
		g->err = errno;
		goto error;
	}

	g->sock = sock;
	return;

error:
	if (addr != NULL)
		freeaddrinfo(addr);
	if (sock >= 0)
		close(sock);
}

static void
tcp_listen_reap(struct lem_async *a)
{
	struct tcp_getaddr *g = (struct tcp_getaddr *)a;
	lua_State *T = g->a.T;
	const char *node = g->node;
	const char *service = g->service;
	int sock = g->sock;
	int err = g->err;

	free(g);

	if (sock >= 0) {
		struct ev_io *w;

		/* create userdata and set the metatable */
		w = lua_newuserdata(T, sizeof(struct ev_io));
		lua_pushvalue(T, 2);
		lua_setmetatable(T, -2);

		/* initialize userdata */
		ev_io_init(w, NULL, sock, EV_READ);
		w->data = NULL;
		lem_queue(T, 1);
		return;
	}

	lua_pushnil(T);
	switch (-sock) {
	case 1:
		lua_pushfstring(T, "error looking up '%s:%s': %s",
				node, service, gai_strerror(err));
		break;
	case 2:
		lua_pushfstring(T, "error creating socket: %s",
				strerror(err));
		break;
	case 3:
		lua_pushfstring(T, "error binding to '%s:%s': %s",
				node, service, strerror(err));
		break;
	case 4:
		lua_pushfstring(T, "error listening on '%s:%s': %s",
				node, service, strerror(err));
		break;

	case 5:
		lua_pushfstring(T, "error making socket non-blocking: %s",
		                strerror(err));
		break;
	}
	lem_queue(T, 2);
}

static int
tcp_listen(lua_State *T)
{
	const char *node = luaL_checkstring(T, 1);
	const char *service = luaL_checkstring(T, 2);
	int family = luaL_checkoption(T, 3, "any", tcp_famnames);
	int backlog = (int)luaL_optnumber(T, 4, MAXPENDING);
	struct tcp_getaddr *g;

	if (node[0] == '*' && node[1] == '\0')
		node = "0.0.0.0";

	g = lem_xmalloc(sizeof(struct tcp_getaddr));
	g->node = node;
	g->service = service;
	g->sock = family;
	g->err = backlog;
	lem_async_do(&g->a, T, tcp_listen_work, tcp_listen_reap);

	lua_settop(T, 1);
	lua_pushvalue(T, lua_upvalueindex(1));
	return lua_yield(T, 2);
}
