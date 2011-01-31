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

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <assert.h>

#include "config.h"

#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"

#include "libev/ev.h"
#include "macros.h"

#ifdef NDEBUG
#define lem_log_error(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else
#define lem_log_error lem_debug
#endif

#define LEM_THREADTABLE 1

struct lem_runqueue_node {
	struct lem_runqueue_node *next;
	lua_State *T;
	int nargs;
};

struct lem_runqueue {
	struct ev_idle w;
	struct lem_runqueue_node *first;
	struct lem_runqueue_node *last;
	int status;
	unsigned error : 1;
};

#if EV_MULTIPLICITY
struct ev_loop *lem_loop;
#endif
static lua_State *L;
static struct lem_runqueue rq;

static
void oom()
{
	static const char e[] = "Out of memory\n";
	
	fprintf(stderr, e);
#ifdef SIGQUIT
	raise(SIGQUIT);
#endif
	_Exit(EXIT_FAILURE);
}

void *
lem_xmalloc(size_t size)
{
	void *p;

	p = malloc(size);
	if (p == NULL)
		oom();

	return p;
}

static int
ignore_sigpipe()
{
	struct sigaction act;

	if (sigaction(SIGPIPE, NULL, &act)) {
		lem_log_error("error getting signal action: %s",
		              strerror(errno));
		return -1;
	}

	act.sa_handler = SIG_IGN;

	if (sigaction(SIGPIPE, &act, NULL)) {
		lem_log_error("error setting signal action: %s",
		              strerror(errno));
		return -1;
	}

	return 0;
}

lua_State *
lem_newthread()
{
	lua_State *T = lua_newthread(L);

	if (T == NULL)
		oom();

	/* set thread_table[T] = true */
	lua_pushboolean(L, 1);
	lua_rawset(L, LEM_THREADTABLE);

	return T;
}

void
lem_forgetthread(lua_State *T)
{
	/* set thread_table[T] = nil */
	lua_pushthread(T);
	lua_xmove(T, L, 1);
	lua_pushnil(L);
	lua_rawset(L, LEM_THREADTABLE);
}

void
lem_queue(lua_State *T, int nargs)
{
	struct lem_runqueue_node *n;

	assert(T != NULL);
	lem_debug("enqueueing thread with %d argument%s",
	              nargs, nargs == 1 ? "" : "s");

	/* create new node */
	n = lem_xmalloc(sizeof(struct lem_runqueue_node));
	n->next = NULL;
	n->T = T;
	n->nargs = nargs;

	/* insert node */
	if (rq.last) {
		rq.last->next = n;
		rq.last = n;
	} else {
		rq.first = rq.last = n;
		ev_idle_start(EV_G_ &rq.w);
	}
}

static void
runqueue_pop(EV_P_ struct ev_idle *w, int revents)
{
	struct lem_runqueue_node *n = rq.first;
	lua_State *T;
	int nargs;

	(void)revents;

	if (n == NULL) { /* queue is empty */
		lem_debug("runqueue is empty");
		ev_idle_stop(EV_A_ w);
		return;
	}

	lem_debug("running thread...");

	T = n->T;
	nargs = n->nargs;

	/* dequeue first node */
	rq.first = n->next;
	if (n->next == NULL)
		rq.last = NULL;
	
	free(n);

	/* run Lua thread */
	switch (lua_resume(T, nargs)) {
	case 0: /* thread finished successfully */
		lem_debug("thread finished successfully");

		lem_forgetthread(T);
		return;

	case LUA_YIELD: /* thread yielded */
		lem_debug("thread yielded");
		return;

	case LUA_ERRERR: /* error running error handler */
		lem_debug("thread errored while running error handler");
	case LUA_ERRRUN: /* runtime error */
		lem_debug("thread errored");

		lua_xmove(T, L, 1);
		rq.error = 1;
		rq.status = EXIT_FAILURE;
		ev_unloop(EV_A_ EVUNLOOP_ALL);
		return;

	case LUA_ERRMEM: /* out of memory */
		return oom();

	default: /* this shouldn't happen */
		lem_debug("lua_resume: unknown error");

		lua_pushliteral(L, "unknown error");
		rq.error = 1;
		rq.status = EXIT_FAILURE;
		ev_unloop(EV_A_ EVUNLOOP_ALL);
		return;
	}
}

void
lem_exit(int status)
{
	rq.status = status;

	ev_unloop(EV_G_ EVUNLOOP_ALL);
}

static int
queue_file(int argc, char *argv[], int fidx)
{
	lua_State *T = lem_newthread();
	int i;

	switch (luaL_loadfile(T, argv[fidx])) {
	case 0: /* success */
		break;

	case LUA_ERRMEM:
		oom();

	default:
		lem_log_error("%s", lua_tostring(T, 1));
		return -1;
	}

	lua_createtable(T, argc, 0);
	for (i = 0; i < argc; i++) {
		lua_pushstring(T, argv[i]);
		lua_rawseti(T, -2, i - fidx);
	}
	lua_setglobal(T, "arg");

	lem_queue(T, 0);
	return 0;
}

int
main(int argc, char *argv[])
{
	if (argc < 2) {
		lem_log_error("I need a file..");
		return EXIT_FAILURE;
	}

#if EV_MULTIPLICITY
	lem_loop = ev_default_loop(0);
	if (lem_loop == NULL) {
#else
	if (!ev_default_loop(0)) {
#endif
		lem_log_error("Error initializing event loop");
		return EXIT_FAILURE;
	}

	if (ignore_sigpipe())
		goto error;

	/* create main Lua state */
	L = luaL_newstate();
	if (L == NULL) {
		lem_log_error("Error initializing Lua state");
		goto error;
	}
	luaL_openlibs(L);

	/* push thread table */
	lua_newtable(L);

	/* initialize runqueue */
	ev_idle_init(&rq.w, runqueue_pop);
	ev_idle_start(EV_G_ &rq.w);
	rq.first = rq.last = NULL;
	rq.error = 0;
	rq.status = EXIT_SUCCESS;

	/* load file */
	if (queue_file(argc, argv, 1))
		goto error;

	/* start the mainloop */
	ev_loop(EV_G_ 0);
	lem_debug("event loop exited");

	if (rq.error) {
		/* print error message */
		lem_log_error("%s", lua_tostring(L, lua_gettop(L)));
	}

	/* shutdown Lua */
	lua_close(L);

	/* free runqueue */
	while (rq.first) {
		struct lem_runqueue_node *n = rq.first;

		rq.first = n->next;
		free(n);
	}

	/* close default loop */
	ev_default_destroy();
	lem_debug("Bye o/");
	return rq.status;

error:
	if (L)
		lua_close(L);
	ev_default_destroy();
	return EXIT_FAILURE;
}
