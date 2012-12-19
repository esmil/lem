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

#include <sys/time.h>
#include <lem.h>

static int
sleeper_wakeup(lua_State *T)
{
	struct ev_timer *w;
	lua_State *S;
	int nargs;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	w = lua_touserdata(T, 1);
	S = w->data;
	if (S == NULL) {
		lua_pushnil(T);
		lua_pushliteral(T, "not sleeping");
		return 2;
	}

	ev_timer_stop(LEM_ w);

	nargs = lua_gettop(T) - 1;
	lua_settop(S, 0);
	lua_xmove(T, S, nargs);
	lem_queue(S, nargs);
	w->data = NULL;

	/* return true */
	lua_pushboolean(T, 1);
	return 1;
}

static void
sleep_handler(EV_P_ struct ev_timer *w, int revents)
{
	lua_State *T = w->data;

	(void)revents;

	/* return nil, "timeout" */
	lem_queue(T, 2);
	w->data = NULL;
}

static int
sleeper_sleep(lua_State *T)
{
	struct ev_timer *w;

	luaL_checktype(T, 1, LUA_TUSERDATA);
	w = lua_touserdata(T, 1);
	if (w->data != NULL) {
		lua_pushnil(T);
		lua_pushliteral(T, "busy");
		return 2;
	}

	if (lua_gettop(T) > 1 && !lua_isnil(T, 2)) {
		ev_tstamp delay = (ev_tstamp)luaL_checknumber(T, 2);

		if (delay <= 0) {
			/* return nil, "timeout"
			 * ..but yield the thread */
			lua_pushnil(T);
			lua_pushvalue(T, lua_upvalueindex(1));
			lem_queue(T, 2);

			return lua_yield(T, 2);
		}

		ev_timer_set(w, delay, 0);
		ev_timer_start(LEM_ w);
	}

	w->data = T;

	/* yield sleeper, nil, "timeout" */
	lua_settop(T, 1);
	lua_pushnil(T);
	lua_pushvalue(T, lua_upvalueindex(1));
	return lua_yield(T, 3);
}

static int
sleeper_new(lua_State *T)
{
	struct ev_timer *w;

	/* create new sleeper object and set metatable */
	w = lua_newuserdata(T, sizeof(struct ev_timer));
	lua_pushvalue(T, lua_upvalueindex(1));
	lua_setmetatable(T, -2);

	ev_init(w, sleep_handler);
	w->data = NULL;

	return 1;
}

static int
utils_spawn(lua_State *T)
{
	lua_State *S;
	int nargs;

	luaL_checktype(T, 1, LUA_TFUNCTION);

	S = lem_newthread();
	nargs = lua_gettop(T);

	lua_xmove(T, S, nargs);

	lem_queue(S, nargs - 1);

	lua_pushboolean(T, 1);
	return 1;
}

static int
utils_yield(lua_State *T)
{
	lem_queue(T, 0);
	return lua_yield(T, 0);
}

static int
utils_exit(lua_State *T)
{
	int status = (int)luaL_checknumber(T, 1);
	lem_exit(status);
	return lua_yield(T, 0);
}

static int
utils_thisthread(lua_State *T)
{
	lua_pushthread(T);
	return 1;
}

static int
utils_suspend(lua_State *T)
{
	return lua_yield(T, 0);
}

static int
utils_resume(lua_State *T)
{
	int args;
	lua_State *S;

	luaL_checktype(T, 1, LUA_TTHREAD);
	S = lua_tothread(T, 1);

	args = lua_gettop(T) - 1;
	lua_xmove(T, S, args);
	lem_queue(S, args);

	return 0;
}

static int
utils_now(lua_State *T)
{
	lua_pushnumber(T, (lua_Number)ev_now());
	return 1;
}

static int
utils_updatenow(lua_State *T)
{
	ev_now_update(LEM);
	lua_pushnumber(T, (lua_Number)ev_now());
	return 1;
}

static int
utils_poolconfig(lua_State *T)
{
	lua_Number n;
	int delay;
	int min;
	int max;

	n = luaL_checknumber(T, 1);
	delay = (int)n;
	luaL_argcheck(T, (lua_Number)delay == n && delay >= 0,
			1, "not an integer in proper range");
	n = luaL_checknumber(T, 2);
	min = (int)n;
	luaL_argcheck(T, (lua_Number)min == n && min >= 0,
			2, "not an integer in proper range");
	n = luaL_checknumber(T, 3);
	max = (int)n;
	luaL_argcheck(T, (lua_Number)max == n && max > 0 && max >= min,
			3, "not an integer in proper range");

	lem_async_config(delay, min, max);
	return 0;
}

int
luaopen_lem_utils(lua_State *L)
{
	/* create module table */
	lua_newtable(L);

	/* create new sleeper metatable */
	lua_newtable(L);
	/* mt.__index = mt */
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	/* mt.wakeup = <sleeper_wakeup> */
	lua_pushcfunction(L, sleeper_wakeup);
	lua_setfield(L, -2, "wakeup");
	/* mt.sleep = <sleeper_sleep> */
	lua_pushliteral(L, "timeout"); /* upvalue 1: "timeout" */
	lua_pushcclosure(L, sleeper_sleep, 1);
	lua_setfield(L, -2, "sleep");
	/* set sleeper function */
	lua_pushcclosure(L, sleeper_new, 1);
	lua_setfield(L, -2, "newsleeper");

	/* set spawn function */
	lua_pushcfunction(L, utils_spawn);
	lua_setfield(L, -2, "spawn");

	/* set yield function */
	lua_pushcfunction(L, utils_yield);
	lua_setfield(L, -2, "yield");

	/* set exit function */
	lua_pushcfunction(L, utils_exit);
	lua_setfield(L, -2, "exit");

	/* set thisthread function */
	lua_pushcfunction(L, utils_thisthread);
	lua_setfield(L, -2, "thisthread");
	/* set suspend function */
	lua_pushcfunction(L, utils_suspend);
	lua_setfield(L, -2, "suspend");
	/* set resume function */
	lua_pushcfunction(L, utils_resume);
	lua_setfield(L, -2, "resume");

	/* set now function */
	lua_pushcfunction(L, utils_now);
	lua_setfield(L, -2, "now");
	/* set updatenow function */
	lua_pushcfunction(L, utils_updatenow);
	lua_setfield(L, -2, "updatenow");

	/* set poolconfig function */
	lua_pushcfunction(L, utils_poolconfig);
	lua_setfield(L, -2, "poolconfig");

	return 1;
}
