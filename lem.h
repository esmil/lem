/*
 * This file is part of LEM, a Lua Event Machine.
 * Copyright 2011 Emil Renner Berthing
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

#ifndef _LEM_H
#define _LEM_H

#include <lem/config.h>
#include <lem/ev.h>
#include <lua.h>
#include <lauxlib.h>
#include <lem/macros.h>

#if EV_MULTIPLICITY
extern struct ev_loop *lem_loop;
#endif

void *lem_xmalloc(size_t size);
lua_State *lem_newthread(void);
void lem_forgetthread(lua_State *T);
void lem_sethandler(lua_State *T);
void lem_queue(lua_State *T, int nargs);
void lem_exit(int status);

#endif
