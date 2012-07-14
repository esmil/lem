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

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <assert.h>

#include "lem.h"

#include <lualib.h>

#if EV_USE_KQUEUE
#define LEM_LOOPFLAGS EVBACKEND_KQUEUE
#else
#define LEM_LOOPFLAGS 0
#endif

#ifdef NDEBUG
#define lem_log_error(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else
#define lem_log_error lem_debug
#endif

#ifndef LUA_OK
#define LUA_OK 0
#endif

#define LEM_INITIAL_QUEUESIZE 8 /* this must be a power of 2 */
#define LEM_THREADTABLE 1

struct lem_runqueue_slot {
	lua_State *T;
	int nargs;
};

struct lem_runqueue {
	struct ev_idle w;
	unsigned long first;
	unsigned long last;
	unsigned long mask;
	struct lem_runqueue_slot *queue;
};

#if EV_MULTIPLICITY
struct ev_loop *lem_loop;
#endif
static lua_State *L;
static struct lem_runqueue rq;
static int exit_status = EXIT_SUCCESS;

static void
oom(void)
{
	static const char e[] = "out of memory\n";
	
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
ignore_sigpipe(void)
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
lem_newthread(void)
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
lem_sethandler(lua_State *T)
{
	/* push T to L */
	lua_pushthread(T);
	lua_xmove(T, L, 1);
	/* move handler to L */
	lua_xmove(T, L, 1);
	lua_rawset(L, LEM_THREADTABLE);
}

void
lem_exit(int status)
{
	exit_status = status;
	ev_unloop(LEM_ EVUNLOOP_ALL);
}

void
lem_queue(lua_State *T, int nargs)
{
	struct lem_runqueue_slot *slot;

	assert(T != NULL);
	lem_debug("enqueueing thread with %d argument%s",
	              nargs, nargs == 1 ? "" : "s");

	if (rq.first == rq.last)
		ev_idle_start(LEM_ &rq.w);

	slot = &rq.queue[rq.last];
	slot->T = T;
	slot->nargs = nargs;

	rq.last++;
	rq.last &= rq.mask;
	if (rq.first == rq.last) {
		unsigned long i;
		unsigned long j;
		struct lem_runqueue_slot *new_queue;

		lem_debug("expanding queue to %lu slots", 2*(rq.mask + 1));
		new_queue = lem_xmalloc(2*(rq.mask + 1)
				* sizeof(struct lem_runqueue_slot));

		i = 0;
		j = rq.first;
		do {
			new_queue[i] = rq.queue[j];

			i++;
			j++;
			j &= rq.mask;
		} while (j != rq.first);

		free(rq.queue);
		rq.queue = new_queue;
		rq.first = 0;
		rq.last = i;
		rq.mask = 2*rq.mask + 1;
	}
}

static void
runqueue_pop(EV_P_ struct ev_idle *w, int revents)
{
	struct lem_runqueue_slot *slot;
	lua_State *T;
	int nargs;

	(void)revents;

	if (rq.first == rq.last) { /* queue is empty */
		lem_debug("runqueue is empty");
		ev_idle_stop(EV_A_ w);
		return;
	}

	lem_debug("running thread...");

	slot = &rq.queue[rq.first];
	T = slot->T;
	nargs = slot->nargs;

	rq.first++;
	rq.first &= rq.mask;

	/* run Lua thread */
#if LUA_VERSION_NUM >= 502
	switch (lua_resume(T, NULL, nargs)) {
#else
	switch (lua_resume(T, nargs)) {
#endif
	case LUA_OK: /* thread finished successfully */
		lem_debug("thread finished successfully");
		lem_forgetthread(T);
		return;

	case LUA_YIELD: /* thread yielded */
		lem_debug("thread yielded");
		return;

	case LUA_ERRERR: /* error running error handler */
		lem_debug("thread errored while running error handler");
#if LUA_VERSION_NUM >= 502
	case LUA_ERRGCMM:
		lem_debug("error in __gc metamethod");
#endif
	case LUA_ERRRUN: /* runtime error */
		lem_debug("thread errored");

		/* push T to L */
		lua_pushthread(T);
		lua_xmove(T, L, 1);

		/* push thread_table[T] */
		lua_pushvalue(L, -1);
		lua_rawget(L, LEM_THREADTABLE);
		if (lua_type(L, -1) == LUA_TFUNCTION) {
			lua_State *S = lem_newthread();

			/* move error handler to S */
			lua_xmove(L, S, 1);
			/* move error message to S */
			lua_xmove(T, S, 1);
			/* queue thread */
			lem_debug("queueing error handler: %s",
			          lua_tostring(S, -1));
			lem_queue(S, 1);

			/* thread_table[T] = nil */
			lua_pushnil(L);
			lua_rawset(L, LEM_THREADTABLE);
			return;
		}
		lem_debug("no error handler");
		/* move error message to L */
		lua_xmove(T, L, 1);
		break;

	case LUA_ERRMEM: /* out of memory */
		oom();

	default: /* this shouldn't happen */
		lem_debug("lua_resume: unknown error");
		lua_pushliteral(L, "unknown error");
		break;
	}
	lem_exit(EXIT_FAILURE);
}

static int
queue_file(int argc, char *argv[], int fidx)
{
	lua_State *T = lem_newthread();
	int i;

	switch (luaL_loadfile(T, argv[fidx])) {
	case LUA_OK: /* success */
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
	lem_loop = ev_default_loop(LEM_LOOPFLAGS);
	if (lem_loop == NULL) {
#else
	if (!ev_default_loop(LEM_LOOPFLAGS)) {
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
	ev_idle_start(LEM_ &rq.w);
	rq.queue = lem_xmalloc(LEM_INITIAL_QUEUESIZE
			* sizeof(struct lem_runqueue_slot));
	rq.first = rq.last = 0;
	rq.mask = LEM_INITIAL_QUEUESIZE - 1;

	/* load file */
	if (queue_file(argc, argv, 1))
		goto error;

	/* start the mainloop */
	ev_loop(LEM_ 0);
	lem_debug("event loop exited");

	/* if there is an error message left on L print it */
	if (lua_type(L, -1) == LUA_TSTRING)
		lem_log_error("%s", lua_tostring(L, -1));

	/* shutdown Lua */
	lua_close(L);

	/* free runqueue */
	free(rq.queue);

	/* close default loop */
	ev_default_destroy();
	lem_debug("Bye %s", exit_status == EXIT_SUCCESS ? "o/" : ":(");
	return exit_status;

error:
	if (L)
		lua_close(L);
	if (rq.queue)
		free(rq.queue);
	ev_default_destroy();
	return EXIT_FAILURE;
}
