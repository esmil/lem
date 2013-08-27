/*
 * This file is part of LEM, a Lua Event Machine.
 * Copyright 2011-2012 Emil Renner Berthing
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

#ifndef _LEM_H
#define _LEM_H

#include <ev.h>
#include <lua.h>
#include <lauxlib.h>

/* Support gcc's __FUNCTION__ for people using other compilers */
#if !defined(__GNUC__) && !defined(__FUNCTION__)
# define __FUNCTION__ __func__ /* C99 */
#endif

/* Built-time assertions */
#define LEM_BUILD_ASSERT__(prefix, line) prefix##line
#define LEM_BUILD_ASSERT_(prefix, line) LEM_BUILD_ASSERT__(prefix, line)
#define LEM_BUILD_ASSERT(x) \
    typedef int LEM_BUILD_ASSERT_(lem_assert_, __LINE__)[(x) ? 1 : -1]

#ifdef NDEBUG
#define lem_debug(...)
#else
#define lem_debug(fmt, ...) do { \
	printf("%s (%s:%u): " fmt "\n", \
		__FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__); \
	fflush(stdout); } while (0)
#endif

#if EV_MINPRI == EV_MAXPRI
# undef ev_priority
# undef ev_set_priority
# define ev_priority(pri)
# define ev_set_priority(ev, pri)
#endif

#if EV_MULTIPLICITY
extern struct ev_loop *lem_loop;
# define LEM lem_loop
# define LEM_ LEM,
#else
# define LEM
# define LEM_
#endif

struct lem_async {
	void (*work)(struct lem_async *a);
	void (*reap)(struct lem_async *a);
	struct lem_async *next;
};

void *lem_xmalloc(size_t size);
lua_State *lem_newthread(void);
void lem_forgetthread(lua_State *T);
void lem_queue(lua_State *T, int nargs);
void lem_exit(int status);
void lem_async_run(struct lem_async *a);
void lem_async_config(int delay, int min, int max);

static inline void
lem_async_do(struct lem_async *a,
		void (*work)(struct lem_async *a),
		void (*reap)(struct lem_async *a))
{
	a->work = work;
	a->reap = reap;
	lem_async_run(a);
}

#endif
