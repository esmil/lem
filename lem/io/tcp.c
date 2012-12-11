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

static void
connect_handler(EV_P_ struct ev_io *w, int revents)
{
	(void)revents;

	lem_debug("connection established");
	ev_io_stop(EV_A_ w);
	lem_queue(w->data, 2);
	w->data = NULL;
}

static int
tcp_connect(lua_State *T)
{
	const char *addr = luaL_checkstring(T, 1);
	uint16_t port = (uint16_t)luaL_checknumber(T, 2);
	struct addrinfo hints = {
		.ai_flags     = 0,
		.ai_family    = AF_UNSPEC,
		.ai_socktype  = SOCK_STREAM,
		.ai_protocol  = IPPROTO_TCP,
		.ai_addrlen   = 0,
		.ai_addr      = NULL,
		.ai_canonname = NULL,
		.ai_next      = NULL
	};
	struct addrinfo *ainfo;
	int sock;
	int ret;
	struct istream *is;
	struct ostream *os;

	/* lookup name */
	ret = getaddrinfo(addr, NULL, &hints, &ainfo);
	if (ret != 0) {
		lua_pushnil(T);
		lua_pushfstring(T, "error looking up \"%s\": %s",
		                addr, gai_strerror(ret));
		return 2;
	}

	/* create the TCP socket */
	switch (ainfo->ai_family) {
	case AF_INET:
		((struct sockaddr_in *)ainfo->ai_addr)->sin_port = htons(port);
		sock = socket(PF_INET, ainfo->ai_socktype, ainfo->ai_protocol);
		break;

	case AF_INET6:
		((struct sockaddr_in6 *)ainfo->ai_addr)->sin6_port = htons(port);
		sock = socket(PF_INET6, ainfo->ai_socktype, ainfo->ai_protocol);
		break;

	default:
		freeaddrinfo(ainfo);
		lua_pushnil(T);
		lua_pushfstring(T, "getaddrinfo() returned neither "
		                "IPv4 or IPv6 address for \"%s\"",
				addr);
		return 2;
	}

	lem_debug("sock = %d", sock);

	if (sock < 0) {
		freeaddrinfo(ainfo);
		lua_pushnil(T);
		lua_pushfstring(T, "error creating TCP socket: %s",
		                strerror(errno));
		return 2;
	}

	/* make the socket non-blocking */
	if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
		close(sock);
		freeaddrinfo(ainfo);
		lua_pushnil(T);
		lua_pushfstring(T, "error making socket non-blocking: %s",
		                strerror(errno));
		return 2;
	}

	lua_settop(T, 1);
	is = istream_new(T, sock, lua_upvalueindex(1));
	os = ostream_new(T, sock, lua_upvalueindex(2));
	is->twin = os;
	os->twin = is;

	/* connect */
	ret = connect(sock, ainfo->ai_addr, ainfo->ai_addrlen);
	freeaddrinfo(ainfo);
	if (ret == 0) {
		lem_debug("connection established");
		return 2;
	}

	if (errno == EINPROGRESS) {
		lem_debug("EINPROGRESS");
		os->w.data = T;
		os->w.cb = connect_handler;
		ev_io_start(LEM_ &os->w);
		return lua_yield(T, 3);
	}

	close(sock);
	lua_pushnil(T);
	lua_pushfstring(T, "error connecting to %s:%d: %s",
			addr, (int)port, strerror(errno));
	return 2;
}

static int
common_listen(lua_State *T, struct sockaddr *address, socklen_t alen,
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

	return common_listen(T, (struct sockaddr *)&address,
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

	return common_listen(T, (struct sockaddr *)&address,
	                     sizeof(struct sockaddr_in6), sock, backlog);
}
