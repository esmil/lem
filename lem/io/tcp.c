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

static int
tcp_listen(lua_State *T, struct sockaddr *address, socklen_t alen,
              int sock, int backlog)
{
	struct ev_io *w;
	int optval = 1;

	/* set SO_REUSEADDR option */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
	               &optval, sizeof(int))) {
		close(sock);
		lua_pushnil(T);
		lua_pushfstring(T, "error setting SO_REUSEADDR on socket: %s",
		                strerror(errno));
		return 2;
	}

	/* make the socket non-blocking */
	if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
		close(sock);
		lua_pushnil(T);
		lua_pushfstring(T, "error making socket non-blocking: %s",
		                strerror(errno));
		return 2;
	}

	/* bind */
	if (bind(sock, address, alen)) {
		close(sock);
		lua_pushnil(T);
		lua_pushfstring(T, "error binding socket: %s",
		                strerror(errno));
		return 2;
	}

	/* listen to the socket */
	if (listen(sock, backlog) < 0) {
		lua_pushnil(T);
		lua_pushfstring(T, "error listening to the socket: %s",
		                strerror(errno));
		return 2;
	}

	/* create userdata and set the metatable */
	w = lua_newuserdata(T, sizeof(struct ev_io));
	lua_pushvalue(T, lua_upvalueindex(1));
	lua_setmetatable(T, -2);

	/* initialize userdata */
	ev_io_init(w, NULL, sock, EV_READ);
	w->data = NULL;

	return 1;
}

static int
tcp4_listen(lua_State *T)
{
	const char *addr = luaL_checkstring(T, 1);
	uint16_t port = (uint16_t)luaL_checknumber(T, 2);
	int backlog = (int)luaL_optnumber(T, 3, MAXPENDING);
	int sock;
	struct sockaddr_in address;

	/* initialise the socketadr_in structure */
	memset(&address, 0, sizeof(struct sockaddr_in));
	address.sin_family = AF_INET;
	if (addr[0] == '*' && addr[1] == '\0')
		address.sin_addr.s_addr = INADDR_ANY;
	else if (!inet_pton(AF_INET, addr, &address.sin_addr)) {
		lua_pushnil(T);
		lua_pushfstring(T, "cannot bind to '%s'", addr);
		return 2;
	}
	address.sin_port = htons(port);

	/* create the TCP socket */
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		lua_pushnil(T);
		lua_pushfstring(T, "error creating TCP socket: %s",
		                strerror(errno));
		return 2;
	}

	return tcp_listen(T, (struct sockaddr *)&address,
	                     sizeof(struct sockaddr_in), sock, backlog);
}

static int
tcp6_listen(lua_State *T)
{
	const char *addr = luaL_checkstring(T, 1);
	uint16_t port = (uint16_t)luaL_checknumber(T, 2);
	int backlog = (int)luaL_optnumber(T, 3, MAXPENDING);
	int sock;
	struct sockaddr_in6 address;

	/* initialise the socketadr_in structure */
	memset(&address, 0, sizeof(struct sockaddr_in6));
	address.sin6_family = AF_INET6;
	if (addr[0] == '*' && addr[1] == '\0')
		address.sin6_addr = in6addr_any;
	else if (!inet_pton(AF_INET6, addr, &address.sin6_addr)) {
		lua_pushnil(T);
		lua_pushfstring(T, "cannot bind to '%s'", addr);
		return 2;
	}
	address.sin6_port = htons(port);

	/* create the TCP socket */
	sock = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		lua_pushnil(T);
		lua_pushfstring(T, "error creating TCP socket: %s",
		                strerror(errno));
		return 2;
	}

	return tcp_listen(T, (struct sockaddr *)&address,
	                     sizeof(struct sockaddr_in6), sock, backlog);
}
