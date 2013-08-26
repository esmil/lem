/*
 * This file is part of LEM, a Lua Event Machine.
 * Copyright 2012 Emil Renner Berthing
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

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <utime.h>
#include <assert.h>

#include <lem.h>

static int
lfs_closed(lua_State *T)
{
	lua_pushnil(T);
	lua_pushliteral(T, "closed");
	return 2;
}

static int
lfs_busy(lua_State *T)
{
	lua_pushnil(T);
	lua_pushliteral(T, "busy");
	return 2;
}

static int
lfs_strerror(lua_State *T, int err)
{
	lua_pushnil(T);
	lua_pushstring(T, strerror(err));
	return 2;
}

struct lfs_pathop {
	struct lem_async a;
	lua_State *T;
	union {
		const char *path;
		int ret;
	};
};

static void
lfs_pathop_reap(struct lem_async *a)
{
	struct lfs_pathop *po = (struct lfs_pathop *)a;
	lua_State *T = po->T;
	int ret = po->ret;

	free(po);
	if (ret) {
		lem_queue(T, lfs_strerror(T, ret));
		return;
	}

	lua_pushboolean(T, 1);
	lem_queue(T, 1);
}

/*
 * lfs.chdir()
 */
static void
lfs_chdir_work(struct lem_async *a)
{
	struct lfs_pathop *po = (struct lfs_pathop *)a;

	if (chdir(po->path))
		po->ret = errno;
	else
		po->ret = 0;
}

static int
lfs_chdir(lua_State *T)
{
	const char *path = luaL_checkstring(T, 1);
	struct lfs_pathop *po;

	po = lem_xmalloc(sizeof(struct lfs_pathop));
	po->T = T;
	po->path = path;
	lem_async_do(&po->a, lfs_chdir_work, lfs_pathop_reap);

	lua_settop(T, 1);
	return lua_yield(T, 1);
}

/*
 * lfs.mkdir()
 */
static void
lfs_mkdir_work(struct lem_async *a)
{
	struct lfs_pathop *po = (struct lfs_pathop *)a;

	if (mkdir(po->path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP |
				S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH ))
		po->ret = errno;
	else
		po->ret = 0;
}

static int
lfs_mkdir(lua_State *T)
{
	const char *path = luaL_checkstring(T, 1);
	struct lfs_pathop *po;

	po = lem_xmalloc(sizeof(struct lfs_pathop));
	po->T = T;
	po->path = path;
	lem_async_do(&po->a, lfs_mkdir_work, lfs_pathop_reap);

	lua_settop(T, 1);
	return lua_yield(T, 1);
}

/*
 * lfs.rmdir()
 */
static void
lfs_rmdir_work(struct lem_async *a)
{
	struct lfs_pathop *po = (struct lfs_pathop *)a;

	if (rmdir(po->path))
		po->ret = errno;
	else
		po->ret = 0;
}

static int
lfs_rmdir(lua_State *T)
{
	const char *path = luaL_checkstring(T, 1);
	struct lfs_pathop *po;

	po = lem_xmalloc(sizeof(struct lfs_pathop));
	po->T = T;
	po->path = path;
	lem_async_do(&po->a, lfs_rmdir_work, lfs_pathop_reap);

	lua_settop(T, 1);
	return lua_yield(T, 1);
}

/*
 * lfs.remove()
 */
static void
lfs_remove_work(struct lem_async *a)
{
	struct lfs_pathop *po = (struct lfs_pathop *)a;

	if (unlink(po->path))
		po->ret = errno;
	else
		po->ret = 0;
}

static int
lfs_remove(lua_State *T)
{
	const char *path = luaL_checkstring(T, 1);
	struct lfs_pathop *po;

	po = lem_xmalloc(sizeof(struct lfs_pathop));
	po->T = T;
	po->path = path;
	lem_async_do(&po->a, lfs_remove_work, lfs_pathop_reap);

	lua_settop(T, 1);
	return lua_yield(T, 1);
}

/*
 * lfs.link() and lfs.rename()
 */
struct lfs_twoop {
	struct lem_async a;
	lua_State *T;
	union {
		struct {
			const char *old;
			const char *new;
		};
		int ret;
	};
};

static void
lfs_twoop_reap(struct lem_async *a)
{
	struct lfs_twoop *to = (struct lfs_twoop *)a;
	lua_State *T = to->T;
	int ret = to->ret;

	free(to);
	if (ret) {
		lem_queue(T, lfs_strerror(T, ret));
		return;
	}

	lua_pushboolean(T, 1);
	lem_queue(T, 1);
}

static void
lfs_link_work(struct lem_async *a)
{
	struct lfs_twoop *to = (struct lfs_twoop *)a;

	if (link(to->old, to->new))
		to->ret = errno;
	else
		to->ret = 0;
}

static void
lfs_symlink_work(struct lem_async *a)
{
	struct lfs_twoop *to = (struct lfs_twoop *)a;

	if (symlink(to->old, to->new))
		to->ret = errno;
	else
		to->ret = 0;
}

static int
lfs_link(lua_State *T)
{
	const char *old = luaL_checkstring(T, 1);
	const char *new = luaL_checkstring(T, 2);
	int symlink = lua_toboolean(T, 3);
	struct lfs_twoop *to;

	to = lem_xmalloc(sizeof(struct lfs_twoop));
	to->T = T;
	to->old = old;
	to->new = new;
	lem_async_do(&to->a, symlink ? lfs_symlink_work : lfs_link_work,
			lfs_twoop_reap);

	lua_settop(T, 2);
	return lua_yield(T, 2);
}

static void
lfs_rename_work(struct lem_async *a)
{
	struct lfs_twoop *to = (struct lfs_twoop *)a;

	if (rename(to->old, to->new))
		to->ret = errno;
	else
		to->ret = 0;
}

static int
lfs_rename(lua_State *T)
{
	const char *old = luaL_checkstring(T, 1);
	const char *new = luaL_checkstring(T, 2);
	struct lfs_twoop *to;

	to = lem_xmalloc(sizeof(struct lfs_twoop));
	to->T = T;
	to->old = old;
	to->new = new;
	lem_async_do(&to->a, lfs_rename_work, lfs_twoop_reap);

	lua_settop(T, 2);
	return lua_yield(T, 2);
}

/*
 * lfs.attributes() and lfs.symlinkattributes()
 */
struct lfs_attr {
	struct lem_async a;
	struct stat st;
	lua_State *T;
	const char *path;
	int op;
	int ret;
};

static void
lfs_stat_work(struct lem_async *a)
{
	struct lfs_attr *at = (struct lfs_attr *)a;

	if (stat(at->path, &at->st))
		at->ret = errno;
	else
		at->ret = 0;
}

static void
lfs_lstat_work(struct lem_async *a)
{
	struct lfs_attr *at = (struct lfs_attr *)a;

	if (lstat(at->path, &at->st))
		at->ret = errno;
	else
		at->ret = 0;
}

static const char *const lfs_attrs[] = {
	"dev",
	"ino",
	"mode",
	"permissions",
	"nlink",
	"uid",
	"gid",
	"rdev",
	"size",
	"blksize",
	"blocks",
	"access",
	"modification",
	"change",
	"*", NULL };

static void
lfs_attr_pushmode(lua_State *T, mode_t mode)
{
	if (S_ISREG(mode))
		lua_pushliteral(T, "file");
	else if (S_ISDIR(mode))
		lua_pushliteral(T, "directory");
	else if (S_ISLNK(mode))
		lua_pushliteral(T, "link");
	else if (S_ISSOCK(mode))
		lua_pushliteral(T, "socket");
	else if (S_ISFIFO(mode))
		lua_pushliteral(T, "named pipe");
	else if (S_ISCHR(mode))
		lua_pushliteral(T, "char device");
	else if (S_ISBLK(mode))
		lua_pushliteral(T, "block device");
	else
		lua_pushliteral(T, "other");
}

static void
lfs_attr_pushperm(lua_State *T, mode_t mode)
{
	static const char sign[3] = { 'r', 'w', 'x' };
	char str[9];
	mode_t mask = 0400;
	int i;

	for (i = 0; i < 9; i++) {
		if (mode & mask)
			str[i] = sign[i % 3];
		else
			str[i] = '-';
		mask >>= 1;
	}

	lua_pushlstring(T, str, 9);
}

static void
lfs_attr_push(lua_State *T, struct stat *st, int i)
{
	switch (i) {
	case 0:  lua_pushnumber(T, st->st_dev); break;
	case 1:  lua_pushnumber(T, st->st_ino); break;
	case 2:  lfs_attr_pushmode(T, st->st_mode); break;
	case 3:  lfs_attr_pushperm(T, st->st_mode); break;
	case 4:  lua_pushnumber(T, st->st_nlink); break;
	case 5:  lua_pushnumber(T, st->st_uid); break;
	case 6:  lua_pushnumber(T, st->st_gid); break;
	case 7:  lua_pushnumber(T, st->st_rdev); break;
	case 8:  lua_pushnumber(T, st->st_size); break;
	case 9:  lua_pushnumber(T, st->st_blksize); break;
	case 10: lua_pushnumber(T, st->st_blocks); break;
	case 11: lua_pushnumber(T, st->st_atime); break;
	case 12: lua_pushnumber(T, st->st_mtime); break;
	case 13: lua_pushnumber(T, st->st_ctime); break;
	}
}

static void
lfs_attr_reap(struct lem_async *a)
{
	struct lfs_attr *at = (struct lfs_attr *)a;
	lua_State *T = at->T;
	struct stat *st = &at->st;

	if (at->ret) {
		lem_queue(T, lfs_strerror(T, at->ret));
		free(at);
		return;
	}

	if (at->op == 14) {
		int i;

		lua_createtable(T, 0, 14);
		for (i = 0; i < 14; i++) {
			lfs_attr_push(T, st, i);
			lua_setfield(T, -2, lfs_attrs[i]);
		}
	} else
		lfs_attr_push(T, st, at->op);

	free(at);
	lem_queue(T, 1);
}

static int
lfs_attr(lua_State *T)
{
	const char *path = luaL_checkstring(T, 1);
	int op = luaL_checkoption(T, 2, "*", lfs_attrs);
	struct lfs_attr *at;

	at = lem_xmalloc(sizeof(struct lfs_attr));
	at->T = T;
	at->path = path;
	at->op = op;
	lem_async_do(&at->a, lfs_stat_work, lfs_attr_reap);

	lua_settop(T, 1);
	return lua_yield(T, 1);
}

static int
lfs_symattr(lua_State *T)
{
	const char *path = luaL_checkstring(T, 1);
	struct lfs_attr *at;

	at = lem_xmalloc(sizeof(struct lfs_attr));
	at->T = T;
	at->path = path;
	lem_async_do(&at->a, lfs_lstat_work, lfs_attr_reap);

	lua_settop(T, 1);
	return lua_yield(T, 1);
}

/*
 * lfs.touch()
 */
struct lfs_touch {
	struct lem_async a;
	lua_State *T;
	union {
		struct {
			struct utimbuf utb;
			struct utimbuf *buf;
			const char *path;
		};
		int ret;
	};
};

static void
lfs_touch_work(struct lem_async *a)
{
	struct lfs_touch *t = (struct lfs_touch *)a;

	if (utime(t->path, t->buf))
		t->ret = errno;
	else
		t->ret = 0;
}

static void
lfs_touch_reap(struct lem_async *a)
{
	struct lfs_touch *t = (struct lfs_touch *)a;
	lua_State *T = t->T;
	int ret = t->ret;

	free(t);
	if (ret) {
		lem_queue(T, lfs_strerror(T, ret));
		return;
	}

	lua_pushboolean(T, 1);
	lem_queue(T, 1);
}

static int
lfs_touch(lua_State *T)
{
	const char *path = luaL_checkstring(T, 1);
	struct lfs_touch *t;

	t = lem_xmalloc(sizeof(struct lfs_touch));
	t->T = T;
	t->path = path;
	if (lua_gettop(T) == 1) {
		t->buf = NULL;
	} else {
		t->utb.actime  = luaL_optnumber(T, 2, 0);
		t->utb.modtime = luaL_optnumber(T, 3, t->utb.actime);
		t->buf = &t->utb;
	}
	lem_async_do(&t->a, lfs_touch_work, lfs_touch_reap);

	lua_settop(T, 1);
	return lua_yield(T, 1);
}

/*
 * lfs.currentdir()
 */
static int
lfs_currentdir(lua_State *T)
{
	char stbuf[128];
	size_t len = 128;
	char *buf = NULL;
	char *path;

	path = getcwd(stbuf, len);
	while (path == NULL && errno == ERANGE) {
		free(buf);
		len *= 2;
		buf = lem_xmalloc(len);
		path = getcwd(buf, len);
	}

	if (path == NULL) {
		free(buf);
		return lfs_strerror(T, errno);
	}

	lua_pushstring(T, path);
	free(buf);
	return 1;
}

/*
 * dir
 */
struct lfs_dir {
	struct lem_async a;
	lua_State *T;
	DIR *handle;
	struct dirent *entry;
	union {
		const char *path;
		int ret;
	};
};

/*
 * dir:__gc()
 */
static int
lfs_dir_gc(lua_State *T)
{
	struct lfs_dir *d = lua_touserdata(T, 1);

	if (d->handle != NULL)
		(void)closedir(d->handle);

	return 0;
}

/*
 * dir:close()
 */
static int
lfs_dir_close(lua_State *T)
{
	struct lfs_dir *d;
	int ret;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	d = lua_touserdata(T, 1);
	if (d->handle == NULL)
		return lfs_closed(T);
	if (d->T != NULL)
		return lfs_busy(T);

	ret = closedir(d->handle);
	d->handle = NULL;
	if (ret)
		return lfs_strerror(T, errno);

	lua_pushboolean(T, 1);
	return 1;
}

/*
 * dir:next()
 */
static void
lfs_dir_next_work(struct lem_async *a)
{
	struct lfs_dir *d = (struct lfs_dir *)a;

	errno = 0;
	d->entry = readdir(d->handle);
	d->ret = errno;

	if (d->entry == NULL) {
		int ret = closedir(d->handle);
		d->handle = NULL;
		if (ret)
			d->ret = errno;
	}
}

static void
lfs_dir_next_reap(struct lem_async *a)
{
	struct lfs_dir *d = (struct lfs_dir *)a;
	lua_State *T = d->T;

	d->T = NULL;
	if (d->ret) {
		lem_queue(T, lfs_strerror(T, d->ret));
		return;
	}

	if (d->entry == NULL)
		lua_pushnil(T);
	else
		lua_pushstring(T, d->entry->d_name);
	lem_queue(T, 1);
}

static int
lfs_dir_next(lua_State *T)
{
	struct lfs_dir *d;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	d = lua_touserdata(T, 1);
	if (d->handle == NULL)
		return lfs_closed(T);
	if (d->T != NULL)
		return lfs_busy(T);

	d->T = T;
	lem_async_do(&d->a, lfs_dir_next_work, lfs_dir_next_reap);

	lua_settop(T, 1);
	return lua_yield(T, 1);
}

/*
 * lfs.dir()
 */
static void
lfs_dir_work(struct lem_async *a)
{
	struct lfs_dir *d = (struct lfs_dir *)a;

	d->handle = opendir(d->path);
	if (d->handle == NULL)
		d->ret = errno;
	else
		d->ret = 0;
}

static void
lfs_dir_reap(struct lem_async *a)
{
	struct lfs_dir *d = (struct lfs_dir *)a;
	lua_State *T = d->T;

	d->T = NULL;
	if (d->ret)
		lem_queue(T, lfs_strerror(T, d->ret));
	else
		lem_queue(T, 2);
}

static int
lfs_dir(lua_State *T)
{
	const char *path = luaL_checkstring(T, 1);
	struct lfs_dir *d;

	lua_settop(T, 1);
	lua_pushvalue(T, lua_upvalueindex(1));

	/* create dir object and set metatable */
	d = lua_newuserdata(T, sizeof(struct lfs_dir));
	lua_pushvalue(T, lua_upvalueindex(2));
	lua_setmetatable(T, -2);

	d->T = T;
	d->handle = NULL;
	d->path = path;
	lem_async_do(&d->a, lfs_dir_work, lfs_dir_reap);

	return lua_yield(T, 3);
}

int
luaopen_lem_lfs_core(lua_State *L)
{
	/* create module table */
	lua_newtable(L);

	/* push dir:next() method */
	lua_pushcfunction(L, lfs_dir_next);

	/* create dir object metatable */
	lua_newtable(L);
	/* mt.__index = mt */
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	/* mt.__gc = <lfs_dir_gc> */
	lua_pushcfunction(L, lfs_dir_gc);
	lua_setfield(L, -2, "__gc");
	/* mt.close = <lfs_dir_close> */
	lua_pushcfunction(L, lfs_dir_close);
	lua_setfield(L, -2, "close");
	/* mt.next = <lfs_dir_next> */
	lua_pushvalue(L, -2); /* already on the stack */
	lua_setfield(L, -2, "next");

	/* insert dir function      */
	/* upvalue 1: next function */
	/* upvalue 2: dir metatable */
	lua_pushcclosure(L, lfs_dir, 2);
	lua_setfield(L, -2, "dir");

	/* insert chdir function */
	lua_pushcfunction(L, lfs_chdir);
	lua_setfield(L, -2, "chdir");
	/* insert mkdir function */
	lua_pushcfunction(L, lfs_mkdir);
	lua_setfield(L, -2, "mkdir");
	/* insert rmdir function */
	lua_pushcfunction(L, lfs_rmdir);
	lua_setfield(L, -2, "rmdir");
	/* insert remove function */
	lua_pushcfunction(L, lfs_remove);
	lua_setfield(L, -2, "remove");

	/* insert link function */
	lua_pushcfunction(L, lfs_link);
	lua_setfield(L, -2, "link");
	/* insert rename function */
	lua_pushcfunction(L, lfs_rename);
	lua_setfield(L, -2, "rename");

	/* insert attributes function */
	lua_pushcfunction(L, lfs_attr);
	lua_setfield(L, -2, "attributes");
	/* insert attributes function */
	lua_pushcfunction(L, lfs_symattr);
	lua_setfield(L, -2, "symlinkattributes");

	/* insert touch function */
	lua_pushcfunction(L, lfs_touch);
	lua_setfield(L, -2, "touch");

	/* insert currentdir function */
	lua_pushcfunction(L, lfs_currentdir);
	lua_setfield(L, -2, "currentdir");

	return 1;
}
