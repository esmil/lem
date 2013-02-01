/*
 * This file is part of LEM, a Lua Event Machine.
 * Copyright 2012-2013 Emil Renner Berthing
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

struct file {
	struct lem_async a;
	int fd;
	int ret;
	union {
		struct {
			struct lem_parser *p;
		} readp;
		struct {
			const char *str;
			size_t len;
			int idx;
		} write;
		struct {
			off_t val;
		} size;
		struct {
			off_t offset;
			int whence;
		} seek;
		struct {
			off_t start;
			off_t len;
			short type;
		} lock;
	};
	struct lem_inputbuf buf;
};

static struct file *
file_new(lua_State *T, int fd, int mt)
{
	struct file *f;

	/* create userdata and set the metatable */
	f = lua_newuserdata(T, sizeof(struct file));
	lua_pushvalue(T, mt);
	lua_setmetatable(T, -2);

	/* initialize userdata */
	f->a.T = NULL;
	f->fd = fd;
	lem_inputbuf_init(&f->buf);

	return f;
}

static int
file_gc(lua_State *T)
{
	struct file *f = lua_touserdata(T, 1);

	lem_debug("collecting %p, fd = %d", f, f->fd);
	if (f->fd >= 0)
		(void)close(f->fd);

	return 0;
}

static int
file_closed(lua_State *T)
{
	struct file *f;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	f = lua_touserdata(T, 1);
	lua_pushboolean(T, f->fd < 0);
	return 1;
}

static int
file_close(lua_State *T)
{
	struct file *f;
	int ret;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	f = lua_touserdata(T, 1);
	if (f->fd < 0)
		return io_closed(T);
	if (f->a.T != NULL)
		return io_busy(T);

	lem_debug("collecting %d", f->fd);
	ret = close(f->fd);
	f->fd = -1;
	if (ret)
		return io_strerror(T, errno);

	lua_pushboolean(T, 1);
	return 1;
}

/*
 * file:readp() method
 */
static void
file_readp_work(struct lem_async *a)
{
	struct file *f = (struct file *)a;
	ssize_t bytes = read(f->fd, f->buf.buf + f->buf.end,
			LEM_INPUTBUF_SIZE - f->buf.end);

	lem_debug("read %ld bytes from %d", bytes, f->fd);
	if (bytes > 0) {
		f->ret = 0;
		f->buf.end += bytes;
	} else if (bytes == 0) {
		f->ret = -1;
	} else {
		close(f->fd);
		f->fd = -1;
		f->ret = errno;
	}
}

static void
file_readp_reap(struct lem_async *a)
{
	struct file *f = (struct file *)a;
	lua_State *T = f->a.T;
	int ret;

	if (f->ret) {
		enum lem_preason res = f->ret < 0 ? LEM_PCLOSED : LEM_PERROR;

		f->a.T = NULL;

		if (f->readp.p->destroy &&
				(ret = f->readp.p->destroy(T, &f->buf, res)) > 0) {
			lem_queue(T, ret);
			return;
		}

		lua_pushnil(T);
		if (res == LEM_PCLOSED)
			lua_pushliteral(T, "eof");
		else
			lua_pushstring(T, strerror(errno));
		lem_queue(T, 2);
		return;
	}

	ret = f->readp.p->process(T, &f->buf);
	if (ret > 0) {
		f->a.T = NULL;
		lem_queue(T, ret);
		return;
	}

	lem_async_put(&f->a);
}

static int
file_readp(lua_State *T)
{
	struct file *f;
	struct lem_parser *p;
	int ret;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	ret = lua_type(T, 2);
	if (ret != LUA_TUSERDATA && ret != LUA_TLIGHTUSERDATA)
		return luaL_argerror(T, 2, "expected userdata");

	f = lua_touserdata(T, 1);
	if (f->fd < 0)
		return io_closed(T);
	if (f->a.T != NULL)
		return io_busy(T);

	p = lua_touserdata(T, 2);
	if (p->init)
		p->init(T, &f->buf);

	ret = p->process(T, &f->buf);
	if (ret > 0)
		return ret;

	f->readp.p = p;
	lem_async_do(&f->a, T, file_readp_work, file_readp_reap);
	return lua_yield(T, lua_gettop(T));
}

/*
 * file:write() method
 */
static void
file_write_work(struct lem_async *a)
{
	struct file *f = (struct file *)a;
	ssize_t bytes = write(f->fd, f->write.str, f->write.len);

	if (bytes < 0)
		f->ret = errno;
	else
		f->ret = 0;
}

static void
file_write_reap(struct lem_async *a)
{
	struct file *f = (struct file *)a;
	lua_State *T = f->a.T;
	int top;

	if (f->ret) {
		f->a.T = NULL;
		lem_queue(T, io_strerror(T, f->ret));
		return;
	}

	top = lua_gettop(T);
	do {
		if (f->write.idx == top) {
			f->a.T = NULL;
			lua_pushboolean(T, 1);
			lem_queue(T, 1);
			return;
		}

		f->write.str = lua_tolstring(T, ++f->write.idx, &f->write.len);
	} while (f->write.len == 0);

	lem_async_put(&f->a);
}

static int
file_write(lua_State *T)
{
	struct file *f;
	const char *str;
	size_t len;
	int idx;
	int i;
	int top;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	top = lua_gettop(T);
	idx = 1;
	do {
		str = luaL_checklstring(T, ++idx, &len);
	} while (len == 0 && idx <= top);
	for (i = idx+1; i <= top; i++)
		(void)luaL_checkstring(T, i);

	f = lua_touserdata(T, 1);
	if (f->fd < 0)
		return io_closed(T);
	if (f->a.T != NULL)
		return io_busy(T);
	if (idx > top) {
		lua_pushboolean(T, 1);
		return 1;
	}

	f->write.str = str;
	f->write.len = len;
	f->write.idx = idx;
	lem_async_do(&f->a, T, file_write_work, file_write_reap);

	return lua_yield(T, top);
}

/*
 * file:size() method
 */
static void
file_size_work(struct lem_async *a)
{
	struct file *f = (struct file *)a;
	struct stat st;

	if (fstat(f->fd, &st)) {
		f->ret = errno;
	} else {
		f->ret = 0;
		f->size.val = st.st_size;
	}
}

static void
file_size_reap(struct lem_async *a)
{
	struct file *f = (struct file *)a;
	lua_State *T = f->a.T;

	f->a.T = NULL;

	if (f->ret) {
		lem_queue(T, io_strerror(T, f->ret));
		return;
	}

	lua_pushnumber(T, f->size.val);
	lem_queue(T, 1);
}

static int
file_size(lua_State *T)
{
	struct file *f;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	f = lua_touserdata(T, 1);
	if (f->fd < 0)
		return io_closed(T);
	if (f->a.T != NULL)
		return io_busy(T);

	lem_async_do(&f->a, T, file_size_work, file_size_reap);

	lua_settop(T, 1);
	return lua_yield(T, 1);
}

/*
 * file:seek() method
 */
static void
file_seek_work(struct lem_async *a)
{
	struct file *f = (struct file *)a;
	off_t bytes = lseek(f->fd, f->seek.offset, f->seek.whence);

	if (bytes == (off_t)-1) {
		f->ret = errno;
	} else {
		f->seek.offset = bytes;
		f->ret = 0;
	}
}

static void
file_seek_reap(struct lem_async *a)
{
	struct file *f = (struct file *)a;
	lua_State *T = f->a.T;

	f->a.T = NULL;

	if (f->ret) {
		lem_queue(T, io_strerror(T, f->ret));
		return;
	}

	lua_pushnumber(T, f->seek.offset);
	lem_queue(T, 1);
}

static int
file_seek(lua_State *T)
{
	static const int mode[] = { SEEK_SET, SEEK_CUR, SEEK_END };
	static const char *const modenames[] = { "set", "cur", "end", NULL };
	struct file *f;
	int op;
	lua_Number offset;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	op = luaL_checkoption(T, 2, "cur", modenames);
	offset = luaL_optnumber(T, 3, 0.);
	f = lua_touserdata(T, 1);
	f->seek.offset = (off_t)offset;
	luaL_argcheck(T, (lua_Number)f->seek.offset == offset, 3,
			"not an integer in proper range");
	if (f->fd < 0)
		return io_closed(T);
	if (f->a.T != NULL)
		return io_busy(T);

	/* flush input buffer */
	lem_inputbuf_init(&f->buf);

	f->seek.whence = mode[op];
	lem_async_do(&f->a, T, file_seek_work, file_seek_reap);

	lua_settop(T, 1);
	return lua_yield(T, 1);
}

/*
 * file:lock() method
 */
static void
file_lock_work(struct lem_async *a)
{
	struct file *f = (struct file *)a;
	struct flock fl = {
		.l_type = f->lock.type,
		.l_whence = SEEK_SET,
		.l_start = f->lock.start,
		.l_len = f->lock.len,
	};

	if (fcntl(f->fd, F_SETLK, &fl) == -1)
		f->ret = errno;
	else
		f->ret = 0;
}

static void
file_lock_reap(struct lem_async *a)
{
	struct file *f = (struct file *)a;
	lua_State *T = f->a.T;

	f->a.T = NULL;

	if (f->ret) {
		lem_queue(T, io_strerror(T, f->ret));
		return;
	}

	lua_pushboolean(T, 1);
	lem_queue(T, 1);
}

static int
file_lock(lua_State *T)
{
	static const short mode[] = { F_RDLCK, F_WRLCK, F_UNLCK };
	static const char *const modenames[] = { "r", "w", "u", NULL };
	struct file *f;
	int op;
	lua_Number start;
	lua_Number len;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	op = luaL_checkoption(T, 2, NULL, modenames);
	start = luaL_optnumber(T, 3, 0);
	len = luaL_optnumber(T, 4, 0);
	f = lua_touserdata(T, 1);
	f->lock.start = (off_t)start;
	luaL_argcheck(T, (lua_Number)f->lock.start == start, 3,
			"not an integer in proper range");
	f->lock.len = (off_t)len;
	luaL_argcheck(T, (lua_Number)f->lock.len == len, 4,
			"not an integer in proper range");
	if (f->fd < 0)
		return io_closed(T);
	if (f->a.T != NULL)
		return io_busy(T);

	f->lock.type = mode[op];
	lem_async_do(&f->a, T, file_lock_work, file_lock_reap);

	lua_settop(T, 1);
	return lua_yield(T, 1);
}
