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

struct file {
	struct lem_async a;
	int fd;
	int ret;
	union {
		struct lem_parser *p;
		const char *out;
	};
	size_t out_size;
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
	f->buf.start = f->buf.end = 0;

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
	if (f->fd < 0) {
		lua_pushnil(T);
		lua_pushliteral(T, "already closed");
		return 2;
	}

	if (f->a.T != NULL) {
		lua_pushnil(T);
		lua_pushliteral(T, "busy");
		return 2;
	}

	lem_debug("collecting %d", f->fd);
	ret = close(f->fd);
	f->fd = -1;
	if (ret) {
		lua_pushnil(T);
		lua_pushstring(T, strerror(errno));
		return 2;
	}

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

		if (f->p->destroy && (ret = f->p->destroy(T, &f->buf, res)) > 0) {
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

	ret = f->p->process(T, &f->buf);
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
	if (f->fd < 0) {
		lua_pushnil(T);
		lua_pushliteral(T, "closed");
		return 2;
	}

	if (f->a.T != NULL) {
		lua_pushnil(T);
		lua_pushliteral(T, "busy");
		return 2;
	}

	p = lua_touserdata(T, 2);
	if (p->init)
		p->init(T, &f->buf);

	ret = p->process(T, &f->buf);
	if (ret > 0)
		return ret;

	f->p = p;
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
	ssize_t bytes = write(f->fd, f->out, f->out_size);

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

	f->a.T = NULL;
	if (f->ret) {
		lua_pushnil(T);
		lua_pushstring(T, strerror(f->ret));
		lem_queue(T, 2);
		return;
	}

	lua_pushboolean(T, 1);
	lem_queue(T, 1);
}

static int
file_write(lua_State *T)
{
	struct file *f;
	const char *out;
	size_t out_size;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	out = luaL_checklstring(T, 2, &out_size);

	f = lua_touserdata(T, 1);
	if (f->fd < 0) {
		lua_pushnil(T);
		lua_pushliteral(T, "closed");
		return 2;
	}

	if (f->a.T != NULL) {
		lua_pushnil(T);
		lua_pushliteral(T, "busy");
		return 2;
	}

	f->out = out;
	f->out_size = out_size;
	lem_async_do(&f->a, T, file_write_work, file_write_reap);

	lua_settop(T, 2);
	return lua_yield(T, 2);
}
