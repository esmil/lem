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

#ifndef MAXPENDING
#define MAXPENDING      50
#endif

struct tcp_getaddr {
	struct lem_async a;
	const char *node;
	const char *service;
	int sock;
	int err;
};

static const int tcp_famnumber[] = { AF_UNSPEC, AF_INET, AF_INET6 };
static const char *const tcp_famnames[] = { "any", "ipv4", "ipv6", NULL };

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
	int sock;

	/* lookup name */
	sock = getaddrinfo(g->node, g->service, &hints, &result);
	if (sock) {
		g->sock = -1;
		g->err = sock;
		return;
	}

	/* try the addresses in the order returned */
	for (addr = result; addr; addr = addr->ai_next) {
		sock = socket(addr->ai_family,
#ifdef SOCK_CLOEXEC
				SOCK_CLOEXEC |
#endif
				addr->ai_socktype, addr->ai_protocol);

		lem_debug("addr->ai_family = %d, sock = %d", addr->ai_family, sock);
		if (sock < 0) {
			int err = errno;

			if (err == EAFNOSUPPORT || err == EPROTONOSUPPORT)
				continue;

			g->sock = -2;
			g->err = err;
			goto out;
		}
#ifndef SOCK_CLOEXEC
		if (fcntl(sock, F_SETFD, FD_CLOEXEC) == -1) {
			g->sock = -2;
			g->err = errno;
			goto error;
		}
#endif
		/* connect */
		if (connect(sock, addr->ai_addr, addr->ai_addrlen)) {
			close(sock);
			continue;
		}

		/* make the socket non-blocking */
		if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
			g->sock = -2;
			g->err = errno;
			goto error;
		}

		g->sock = sock;
		goto out;
	}

	g->sock = -3;
	goto out;

error:
	close(sock);
out:
	freeaddrinfo(result);
}

static void
tcp_connect_reap(struct lem_async *a)
{
	struct tcp_getaddr *g = (struct tcp_getaddr *)a;
	lua_State *T = g->a.T;
	int sock = g->sock;

	lem_debug("connection established");
	if (sock >= 0) {
		free(g);

		stream_new(T, sock, 3);
		lem_queue(T, 1);
		return;
	}

	lua_pushnil(T);
	switch (-sock) {
	case 1:
		lua_pushfstring(T, "error looking up '%s:%s': %s",
				g->node, g->service, gai_strerror(g->err));
		break;
	case 2:
		lua_pushfstring(T, "error creating socket: %s",
				strerror(g->err));
		break;
	case 3:
		lua_pushfstring(T, "error connecting to '%s:%s'",
				g->node, g->service);
		break;
	}
	free(g);
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

	lua_settop(T, 2);
	lua_pushvalue(T, lua_upvalueindex(1));
	return lua_yield(T, 3);
}

static void
tcp_listen_work(struct lem_async *a)
{
	struct tcp_getaddr *g = (struct tcp_getaddr *)a;
	struct addrinfo hints = {
		.ai_flags     = AI_PASSIVE,
		.ai_family    = g->sock,
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
		return;
	}

	/* create the TCP socket */
	sock = socket(addr->ai_family,
#ifdef SOCK_CLOEXEC
			SOCK_CLOEXEC |
#endif
			addr->ai_socktype, addr->ai_protocol);
	lem_debug("addr->ai_family = %d, sock = %d", addr->ai_family, sock);
	if (sock < 0) {
		g->sock = -2;
		g->err = errno;
		goto out;
	}
#ifndef SOCK_CLOEXEC
	if (fcntl(sock, F_SETFD, FD_CLOEXEC) == -1) {
		g->sock = -2;
		g->err = errno;
		goto error;
	}
#endif
	/* set SO_REUSEADDR option if possible */
	ret = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &ret, sizeof(int));
#ifdef IPV6_V6ONLY
	if (g->sock == AF_INET6)
		setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &ret, sizeof(int));
#endif

	/* bind */
	if (bind(sock, addr->ai_addr, addr->ai_addrlen)) {
		g->sock = -3;
		g->err = errno;
		goto error;
	}

	/* listen to the socket */
	if (listen(sock, g->err)) {
		g->sock = -4;
		g->err = errno;
		goto error;
	}

	/* make the socket non-blocking */
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		g->sock = -2;
		g->err = errno;
		goto error;
	}

	g->sock = sock;
	goto out;

error:
	close(sock);
out:
	freeaddrinfo(addr);
}

static void
tcp_listen_reap(struct lem_async *a)
{
	struct tcp_getaddr *g = (struct tcp_getaddr *)a;
	lua_State *T = g->a.T;
	int sock = g->sock;

	if (g->node == NULL)
		g->node = "*";

	if (sock >= 0) {
		free(g);
		server_new(T, sock, 3);
		lem_queue(T, 1);
		return;
	}

	lua_pushnil(T);
	switch (-sock) {
	case 1:
		lua_pushfstring(T, "error looking up '%s:%s': %s",
				g->node, g->service, gai_strerror(g->err));
		break;
	case 2:
		lua_pushfstring(T, "error creating socket: %s",
				strerror(g->err));
		break;
	case 3:
		lua_pushfstring(T, "error binding to '%s:%s': %s",
				g->node, g->service, strerror(g->err));
		break;
	case 4:
		lua_pushfstring(T, "error listening on '%s:%s': %s",
				g->node, g->service, strerror(g->err));
		break;
	}
	free(g);
	lem_queue(T, 2);
}

static int
tcp_listen(lua_State *T, int family)
{
	const char *node = luaL_checkstring(T, 1);
	const char *service = luaL_checkstring(T, 2);
	int backlog = (int)luaL_optnumber(T, 3, MAXPENDING);
	struct tcp_getaddr *g;

	if (node[0] == '*' && node[1] == '\0')
		node = NULL;

	g = lem_xmalloc(sizeof(struct tcp_getaddr));
	g->node = node;
	g->service = service;
	g->sock = family;
	g->err = backlog;
	lem_async_do(&g->a, T, tcp_listen_work, tcp_listen_reap);

	lua_settop(T, 2);
	lua_pushvalue(T, lua_upvalueindex(1));
	return lua_yield(T, 3);
}

static int
tcp_listen4(lua_State *T)
{
	return tcp_listen(T, AF_INET);
}

static int
tcp_listen6(lua_State *T)
{
	return tcp_listen(T, AF_INET6);
}
