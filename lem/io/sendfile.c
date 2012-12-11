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

struct lem_sendfile {
	int fd;
	off_t size;
};

static int
sendfile_open(lua_State *T)
{
	const char *path = luaL_checkstring(T, 1);
	int fd;
	struct stat buf;
	struct lem_sendfile *f;

	fd = open(path, O_RDONLY | O_NONBLOCK);
	if (fd < 0)
		return io_strerror(T, errno);

	if (fstat(fd, &buf)) {
		int err = errno;
		close(fd);
		return io_strerror(T, err);
	}

	/* create userdata and set the metatable */
	f = lua_newuserdata(T, sizeof(struct lem_sendfile));
	lua_pushvalue(T, lua_upvalueindex(1));
	lua_setmetatable(T, -2);

	/* initialize userdata */
	f->fd = fd;
	f->size = buf.st_size;

	return 1;
}

static int
sendfile_gc(lua_State *T)
{
	struct lem_sendfile *f = lua_touserdata(T, 1);

	if (f->fd >= 0)
		close(f->fd);

	return 0;
}

static int
sendfile_close(lua_State *T)
{
	struct lem_sendfile *f;
	int ret;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	f = lua_touserdata(T, 1);
	if (f->fd < 0)
		return io_closed(T);

	ret = close(f->fd);
	f->fd = -1;
	if (ret)
		return io_strerror(T, errno);

	lua_pushboolean(T, 1);
	return 1;
}

static int
sendfile_size(lua_State *T)
{
	struct lem_sendfile *f;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	f = lua_touserdata(T, 1);
	if (f->fd < 0)
		return io_closed(T);

	lua_pushnumber(T, (lua_Number)f->size);
	return 1;
}
