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
	if (fd < 0) {
		int err = errno;

		lua_pushnil(T);
		switch (err) {
		case ENOENT:
			lua_pushliteral(T, "not found");
			break;
		case EACCES:
			lua_pushliteral(T, "permission denied");
			break;
		default:
			lua_pushstring(T, strerror(err));
		}
		return 2;
	}

	if (fstat(fd, &buf)) {
		lua_pushnil(T);
		lua_pushstring(T, strerror(errno));
		(void)close(fd);
		return 2;
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

	if (f->fd < 0)
		return 0;

	(void)close(f->fd);
	return 0;
}

static int
sendfile_close(lua_State *T)
{
	struct lem_sendfile *f;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	f = lua_touserdata(T, 1);
	if (f->fd < 0) {
		lua_pushnil(T);
		lua_pushliteral(T, "already closed");
		return 2;
	}

	if (close(f->fd)) {
		lua_pushnil(T);
		lua_pushstring(T, strerror(errno));
		return 2;
	}

	f->fd = -1;
	lua_pushboolean(T, 1);
	return 1;
}

static int
sendfile_size(lua_State *T)
{
	struct lem_sendfile *f;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	f = lua_touserdata(T, 1);
	if (f->fd < 0) {
		lua_pushnil(T);
		lua_pushliteral(T, "closed");
		return 2;
	}

	lua_pushnumber(T, (lua_Number)f->size);
	return 1;
}
