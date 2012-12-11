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

struct ostream;

struct istream {
	ev_io w;
	struct ostream *twin;
	struct lem_parser *p;
	struct lem_inputbuf buf;
};

struct ostream {
	ev_io w;
	struct istream *twin;
	const char *data;
	size_t len;
};

static struct istream *
istream_new(lua_State *T, int fd, int mt)
{
	struct istream *s;

	/* create userdata and set the metatable */
	s = lua_newuserdata(T, sizeof(struct istream));
	lua_pushvalue(T, mt);
	lua_setmetatable(T, -2);

	/* initialize userdata */
	ev_io_init(&s->w, NULL, fd, EV_READ);
	s->w.data = NULL;
	s->twin = NULL;
	s->buf.start = s->buf.end = 0;

	return s;
}

static int
stream_closed(lua_State *T)
{
	struct ev_io *w;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	w = lua_touserdata(T, 1);
	lua_pushboolean(T, w->fd < 0);
	return 1;
}

static int
stream_busy(lua_State *T)
{
	struct ev_io *w;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	w = lua_touserdata(T, 1);
	lua_pushboolean(T, w->data != NULL);
	return 1;
}

static int
stream_interrupt(lua_State *T)
{
	struct ev_io *w;
	lua_State *S;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	w = lua_touserdata(T, 1);
	S = w->data;
	if (S == NULL) {
		lua_pushnil(T);
		lua_pushliteral(T, "not busy");
		return 2;
	}

	lem_debug("interrupting io action");
	ev_io_stop(LEM_ w);
	w->data = NULL;
	lua_settop(S, 0);
	lua_pushnil(S);
	lua_pushliteral(S, "interrupted");
	lem_queue(S, 2);

	lua_pushboolean(T, 1);
	return 1;
}

static int
stream_close_check(lua_State *T, struct ev_io *w)
{
	if (w->fd < 0)
		return io_closed(T);

	if (w->data != NULL) {
		lua_State *S = w->data;

		lem_debug("interrupting io action");
		ev_io_stop(LEM_ w);
		w->data = NULL;
		lua_settop(S, 0);
		lua_pushnil(S);
		lua_pushliteral(S, "interrupted");
		lem_queue(S, 2);
	}

	return 0;
}

static int
istream_close(lua_State *T)
{
	struct istream *s;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	s = lua_touserdata(T, 1);

	lem_debug("collecting %d", s->w.fd);
	if (stream_close_check(T, &s->w))
		return 2;

	if (s->twin)
		s->twin->twin = NULL;
	else if (close(s->w.fd)) {
		s->w.fd = -1;
		lua_pushnil(T);
		lua_pushstring(T, strerror(errno));
		return 2;
	}

	s->w.fd = -1;
	lua_pushboolean(T, 1);
	return 1;
}

static int
ostream_close(lua_State *T)
{
	struct ostream *s;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	s = lua_touserdata(T, 1);

	lem_debug("collecting %d", s->w.fd);
	if (stream_close_check(T, &s->w))
		return 2;

	if (s->twin)
		s->twin->twin = NULL;
	else if (close(s->w.fd)) {
		s->w.fd = -1;
		lua_pushnil(T);
		lua_pushstring(T, strerror(errno));
		return 2;
	}

	s->w.fd = -1;
	lua_pushboolean(T, 1);
	return 1;
}

/*
 * istream:readp() method
 */

static void
stream_readp_handler(EV_P_ ev_io *w, int revents)
{
	struct istream *s = (struct istream *)w;
	lua_State *T = s->w.data;
	ssize_t bytes;
	int ret;
	enum lem_preason reason;
	const char *msg;

	(void)revents;

	while ((bytes = read(s->w.fd, s->buf.buf + s->buf.end,
					LEM_INPUTBUF_SIZE - s->buf.end)) > 0) {
		lem_debug("read %ld bytes from %d", bytes, s->w.fd);

		s->buf.end += bytes;

		ret = s->p->process(T, &s->buf);
		if (ret > 0) {
			ev_io_stop(EV_A_ &s->w);
			s->w.data = NULL;
			lem_queue(T, ret);
			return;
		}
	}
	lem_debug("read %ld bytes from %d", bytes, s->w.fd);

	if (bytes < 0 && errno == EAGAIN)
		return;

	ev_io_stop(EV_A_ &s->w);
	s->w.data = NULL;

	if (s->twin)
		s->twin->twin = NULL;
	else
		(void)close(s->w.fd);
	s->w.fd = -1;

	if (bytes == 0 || errno == ECONNRESET || errno == EPIPE) {
		reason = LEM_PCLOSED;
		msg = "closed";
	} else {
		reason = LEM_PERROR;
		msg = strerror(errno);
	}

	if (s->p->destroy && (ret = s->p->destroy(T, &s->buf, reason)) > 0) {
		lem_queue(T, ret);
		return;
	}

	lua_settop(T, 0);
	lua_pushnil(T);
	lua_pushstring(T, msg);
	lem_queue(T, 2);
}

static int
stream_readp(lua_State *T)
{
	struct istream *s;
	struct lem_parser *p;
	int ret;
	ssize_t bytes;
	enum lem_preason reason;
	const char *msg;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	ret = lua_type(T, 2);
	if (ret != LUA_TUSERDATA && ret != LUA_TLIGHTUSERDATA)
		return luaL_argerror(T, 2, "expected userdata");

	s = lua_touserdata(T, 1);
	if (s->w.fd < 0)
		return io_closed(T);
	if (s->w.data != NULL)
		return io_busy(T);

	p = lua_touserdata(T, 2);
	if (p->init)
		p->init(T, &s->buf);

again:
	ret = p->process(T, &s->buf);
	if (ret > 0)
		return ret;

	bytes = read(s->w.fd, s->buf.buf + s->buf.end, LEM_INPUTBUF_SIZE - s->buf.end);
	lem_debug("read %ld bytes from %d", bytes, s->w.fd);
	if (bytes > 0) {
		s->buf.end += bytes;
		goto again;
	}

	if (bytes < 0 && errno == EAGAIN) {
		s->p = p;
		s->w.data = T;
		s->w.cb = stream_readp_handler;
		ev_io_start(LEM_ &s->w);
		return lua_yield(T, lua_gettop(T));
	}

	if (s->twin)
		s->twin->twin = NULL;
	else
		(void)close(s->w.fd);
	s->w.fd = -1;

	if (bytes == 0 || errno == ECONNRESET || errno == EPIPE) {
		reason = LEM_PCLOSED;
		msg = "closed";
	} else {
		reason = LEM_PERROR;
		msg = strerror(errno);
	}

	if (p->destroy && (ret = p->destroy(T, &s->buf, reason)) > 0)
		return ret;

	lua_settop(T, 0);
	lua_pushnil(T);
	lua_pushstring(T, msg);
	return 2;
}

static struct ostream *
ostream_new(lua_State *T, int fd, int mt)
{
	struct ostream *s;

	/* create userdata and set the metatable */
	s = lua_newuserdata(T, sizeof(struct ostream));
	lua_pushvalue(T, mt);
	lua_setmetatable(T, -2);

	/* initialize userdata */
	ev_io_init(&s->w, NULL, fd, EV_WRITE);
	s->w.data = NULL;
	s->twin = NULL;

	return s;
}

static void
stream_write_handler(EV_P_ struct ev_io *w, int revents)
{
	struct ostream *s = (struct ostream *)w;
	lua_State *T = s->w.data;
	ssize_t bytes;

	(void)revents;

again:
	bytes = write(s->w.fd, s->data, s->len);
	if (bytes > 0) {
		s->len -= bytes;
		if (s->len > 0) {
			s->data += bytes;
			goto again;
		}

		ev_io_stop(EV_A_ &s->w);
		s->w.data = NULL;

		lua_pushboolean(T, 1);
		lem_queue(T, 1);
		return;
	}

	if (bytes < 0 && errno == EAGAIN)
		return;

	ev_io_stop(EV_A_ &s->w);
	s->w.data = NULL;

	lua_pushnil(T);
	if (bytes == 0 || errno == ECONNRESET || errno == EPIPE)
		lua_pushliteral(T, "closed");
	else
		lua_pushstring(T, strerror(errno));

	if (s->twin)
		s->twin->twin = NULL;
	else
		(void)close(s->w.fd);
	s->w.fd = -1;
	lem_queue(T, 2);
}

static int
stream_write(lua_State *T)
{
	struct ostream *s;
	ssize_t bytes;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	luaL_checktype(T, 2, LUA_TSTRING);

	s = lua_touserdata(T, 1);
	if (s->w.fd < 0)
		return io_closed(T);
	if (s->w.data != NULL)
		return io_busy(T);

	s->data = lua_tolstring(T, 2, &s->len);
	if (s->len == 0) {
		lua_pushboolean(T, 1);
		return 1;
	}

again:
	bytes = write(s->w.fd, s->data, s->len);
	if (bytes > 0) {
		s->len -= bytes;
		if (s->len > 0) {
			s->data += bytes;
			goto again;
		}

		lua_pushboolean(T, 1);
		return 1;
	}

	if (bytes < 0 && errno == EAGAIN) {
		lua_settop(T, 1);
		s->w.data = T;
		s->w.cb = stream_write_handler;
		ev_io_start(LEM_ &s->w);
		return lua_yield(T, 1);
	}

	lua_pushnil(T);
	if (bytes == 0 || errno == ECONNRESET || errno == EPIPE)
		lua_pushliteral(T, "closed");
	else
		lua_pushstring(T, strerror(errno));

	if (s->twin)
		s->twin->twin = NULL;
	else
		(void)close(s->w.fd);
	s->w.fd = -1;
	return 2;
}


#ifndef TCP_CORK
#define TCP_CORK TCP_NOPUSH
#endif

static int
stream_setcork(lua_State *T, int state)
{
	struct ostream *s;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	s = lua_touserdata(T, 1);
	if (s->w.fd < 0)
		return io_closed(T);
	if (s->w.data != NULL)
		return io_busy(T);

	if (setsockopt(s->w.fd, IPPROTO_TCP, TCP_CORK, &state, sizeof(int))) {
		lua_pushnil(T);
		lua_pushstring(T, strerror(errno));
		return 2;
	}

	lua_pushboolean(T, 1);
	return 1;
}

static int
stream_cork(lua_State *T)
{
	return stream_setcork(T, 1);
}

static int
stream_uncork(lua_State *T)
{
	return stream_setcork(T, 0);
}

struct sendfile {
	struct lem_sendfile *file;
	off_t offset;
};

static int
try_sendfile(lua_State *T, struct ostream *s, struct sendfile *sf)
{
#ifdef __FreeBSD__
	int ret;

	do {
		size_t count;
		off_t written = 0;

		count = sf->file->size - sf->offset;

		if (count == 0) {
			lua_settop(T, 0);
			lua_pushboolean(T, 1);
			return 1;
		}

		ret = sendfile(sf->file->fd, s->w.fd,
				sf->offset, count,
				NULL, &written, 0);
		lem_debug("wrote = %ld bytes", written);
		sf->offset += written;
	} while (ret >= 0);
#else
#ifdef __APPLE__
	int ret;

	do {
		off_t count = sf->file->size - sf->offset;

		if (count == 0) {
			lua_settop(T, 0);
			lua_pushboolean(T, 1);
			return 1;
		}

		ret = sendfile(sf->file->fd, s->w.fd,
		               sf->offset, &count, NULL, 0);
		lem_debug("wrote = %lld bytes", count);
		sf->offset += count;
	} while (ret >= 0);
#else
	ssize_t ret;

	do {
		size_t count = sf->file->size - sf->offset;

		if (count == 0) {
			lua_settop(T, 0);
			lua_pushboolean(T, 1);
			return 1;
		}

		ret = sendfile(s->w.fd, sf->file->fd,
				&sf->offset,
				count);
		lem_debug("wrote = %ld bytes", ret);
	} while (ret >= 0);
#endif
#endif

	if (errno == EAGAIN)
		return 0;

	lua_pushnil(T);
	lua_pushstring(T, strerror(errno));
	return 2;
}

static void
sendfile_handler(EV_P_ struct ev_io *w, int revents)
{
	struct ostream *s = (struct ostream *)w;
	lua_State *T = s->w.data;
	struct sendfile *sf = lua_touserdata(T, 3);
	int ret;

	(void)revents;

	ret = try_sendfile(T, s, sf);
	if (ret == 0)
		return;

	ev_io_stop(EV_A_ &s->w);
	if (ret == 2) {
		if (s->twin)
			s->twin->twin = NULL;
		else
			(void)close(s->w.fd);
		s->w.fd = -1;
	}

	lem_queue(T, ret);
	s->w.data = NULL;
}

static int
stream_sendfile(lua_State *T)
{
	struct ostream *s;
	struct lem_sendfile *f;
	struct sendfile *sf;
	off_t offset;
	int ret;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	luaL_checktype(T, 2, LUA_TUSERDATA);
	offset = (off_t)luaL_optnumber(T, 3, 0);

	s = lua_touserdata(T, 1);
	if (s->w.fd < 0)
		return io_closed(T);
	if (s->w.data != NULL)
		return io_busy(T);

	f = lua_touserdata(T, 2);
	if (f->fd < 0) {
		lua_pushnil(T);
		lua_pushliteral(T, "file closed");
		return 2;
	}

	if (offset > f->size)
		return luaL_error(T, "offset too big");

	lua_settop(T, 2);
	sf = lua_newuserdata(T, sizeof(struct sendfile));
	sf->file = f;
	sf->offset = offset;

	ret = try_sendfile(T, s, sf);
	if (ret > 0) {
		if (ret == 2) {
			if (s->twin)
				s->twin->twin = NULL;
			else
				(void)close(s->w.fd);
			s->w.fd = -1;
		}
		return ret;
	}

	lem_debug("yielding");
	s->w.data = T;
	s->w.cb = sendfile_handler;
	ev_io_start(LEM_ &s->w);
	return lua_yield(T, 3);
}

struct open {
	struct lem_async a;
	const char *path;
	int fd;
	int flags;
	int type;
};

static void
stream_open_work(struct lem_async *a)
{
	struct open *o = (struct open *)a;
	int fd;
	struct stat st;

	fd = open(o->path, o->flags | O_NONBLOCK,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd < 0)
		goto error;

	if (fstat(fd, &st))
		goto error;

	o->fd = fd;
	lem_debug("st.st_mode & S_IFMT = %o", st.st_mode & S_IFMT);
	switch (st.st_mode & S_IFMT) {
	case S_IFSOCK:
	case S_IFCHR:
	case S_IFIFO:
		o->type = 1;
		break;

	case S_IFREG:
	case S_IFBLK:
		o->type = 0;
		break;

	default:
		o->type = -1;
	}

	return;

error:
	o->fd = -errno;
}

static void
stream_open_reap(struct lem_async *a)
{
	struct open *o = (struct open *)a;
	lua_State *T = o->a.T;
	int fd = o->fd;
	int flags = o->flags;
	int ret = o->type;
	struct istream *is;
	struct ostream *os;

	lem_debug("o->type = %d", ret);
	free(o);

	if (fd < 0) {
		lua_pushnil(T);
		lua_pushstring(T, strerror(-o->fd));
		/*
		switch (-o->fd) {
		case ENOENT:
			lua_pushliteral(T, "not found");
			break;
		case EACCES:
			lua_pushliteral(T, "permission denied");
			break;
		default:
			lua_pushstring(T, strerror(errno));
		}
		*/
		lem_queue(T, 2);
		return;
	}

	if (ret < 0) {
		lua_pushnil(T);
		lua_pushliteral(T, "invalid type");
		lem_queue(T, 2);
		return;
	}

	if (ret == 0) {
		file_new(T, fd, lua_upvalueindex(3));
		lem_queue(T, 1);
		return;
	}

	if ((flags & O_WRONLY) == 0)
		is = istream_new(T, fd, lua_upvalueindex(1));
	else
		is = NULL;

	if (flags & (O_RDWR | O_WRONLY))
		os = ostream_new(T, fd, lua_upvalueindex(2));
	else
		os = NULL;

	if (is && os) {
		is->twin = os;
		os->twin = is;
		ret = 2;
	} else
		ret = 1;

	lem_queue(T, ret);
}

static int
mode_to_flags(const char *mode)
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
stream_open(lua_State *T)
{
	const char *path = luaL_checkstring(T, 1);
	int flags = mode_to_flags(luaL_optstring(T, 2, "r"));
	struct open *o;
	int args;

	if (flags < 0)
		return luaL_error(T, "invalid mode string");

	o = lem_xmalloc(sizeof(struct open));
	o->path = path;
	o->flags = flags;

	lem_async_do(&o->a, T, stream_open_work, stream_open_reap);

	args = lua_gettop(T);
	if (args > 2) {
		lua_settop(T, 2);
		args = 2;
	}
	return lua_yield(T, args);
}

static int
stream_popen(lua_State *T)
{
	const char *cmd = luaL_checkstring(T, 1);
	const char *mode = luaL_optstring(T, 2, "r");
	int fd[2];

	if (mode[0] != 'r' && mode[0] != 'w')
		return luaL_error(T, "invalid mode string");

	if (pipe(fd))
		goto error;

	switch (fork()) {
	case -1: /* error */
		(void)close(fd[0]);
		(void)close(fd[1]);
		goto error;
	case 0: /* child */
		if (mode[0] == 'r') {
			(void)close(fd[0]);
			(void)dup2(fd[1], 1);
		} else {
			(void)close(fd[1]);
			(void)dup2(fd[0], 0);
		}

		(void)execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
		exit(EXIT_FAILURE);
	}

	if (mode[0] == 'r') {
		if (close(fd[1])) {
			(void)close(fd[0]);
			goto error;
		}
	} else {
		if (close(fd[0])) {
			(void)close(fd[1]);
			goto error;
		}
		fd[0] = fd[1];
	}

	/* make the pipe non-blocking */
	if (fcntl(fd[0], F_SETFL, O_NONBLOCK) < 0) {
		(void)close(fd[0]);
		goto error;
	}

	if (mode[0] == 'r')
		(void)istream_new(T, fd[0], lua_upvalueindex(1));
	else
		(void)ostream_new(T, fd[0], lua_upvalueindex(2));
	return 1;
error:
	lua_pushnil(T);
	lua_pushstring(T, strerror(errno));
	return 2;
}
