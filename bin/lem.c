/*
 * This file is part of LEM, a Lua Event Machine.
 * Copyright 2011-2013 Emil Renner Berthing
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
#include <signal.h>
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#include <lem.h>
#include <lualib.h>

#if EV_USE_KQUEUE
#define LEM_LOOPFLAGS (EVFLAG_NOSIGMASK | EVBACKEND_KQUEUE)
#else
#define LEM_LOOPFLAGS EVFLAG_NOSIGMASK
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
	struct lem_runqueue_slot *queue;
	unsigned int first;
	unsigned int last;
	unsigned int mask;
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
setsignal(int signal, void (*handler)(int), int flags)
{
	struct sigaction act;

	act.sa_handler = handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = flags;

	if (sigaction(signal, &act, NULL)) {
		lem_log_error("lem: error setting signal %d: %s",
		              signal, strerror(errno));
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
		unsigned int i;
		unsigned int j;
		struct lem_runqueue_slot *new_queue;

		lem_debug("expanding queue to %u slots", 2*(rq.mask + 1));
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
thread_error(lua_State *T)
{
#ifdef HAVE_TRACEBACK
	const char *msg = lua_tostring(T, -1);

	if (msg)
		luaL_traceback(L, T, msg, 0);
#else /* adapted from Lua 5.1 source */
	if (!lua_isstring(T, -1)) /* 'message' not a string? */
		return;
	lua_getfield(T, LUA_GLOBALSINDEX, "debug");
	if (!lua_istable(T, -1)) {
		lua_pop(T, 1);
		goto merror;
	}
	lua_getfield(T, -1, "traceback");
	if (!lua_isfunction(T, -1)) {
		lua_pop(T, 2);
		goto merror;
	}
	lua_pushvalue(T, -3);  /* pass error message */
	lua_pushinteger(T, 1); /* skip traceback */
	lua_call(T, 2, 1);     /* call debug.traceback */
merror:
	lua_xmove(T, L, 1);    /* move error message to L */
#endif
}

static void
runqueue_pop(EV_P_ struct ev_idle *w, int revents)
{
	struct lem_runqueue_slot *slot;
	lua_State *T;
	int nargs;

	(void)revents;

	if (rq.first == rq.last) { /* queue is empty */
		lem_debug("runqueue is empty, collecting..");
#if 0
		if (lua_gc(L, LUA_GCSTEP, 0)) {
			lem_debug("done collecting");
			ev_idle_stop(EV_A_ w);
		}
#else
		ev_idle_stop(EV_A_ w);
		lua_gc(L, LUA_GCCOLLECT, 0);
#endif
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
		thread_error(T);
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

#include "pool.c"

static int
queue_file(int argc, char *argv[], int fidx)
{
	lua_State *T = lem_newthread();
	const char *filename;
	int i;

	if (fidx < argc)
		filename = argv[fidx];
	else
		filename = LEM_LDIR "lem/repl.lua";

	switch (luaL_loadfile(T, filename)) {
	case LUA_OK: /* success */
		break;

	case LUA_ERRMEM:
		oom();

	default:
		lem_log_error("lem: %s", lua_tostring(T, 1));
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
#if EV_MULTIPLICITY
	lem_loop = ev_default_loop(LEM_LOOPFLAGS);
	if (lem_loop == NULL) {
#else
	if (!ev_default_loop(LEM_LOOPFLAGS)) {
#endif
		lem_log_error("lem: error initializing event loop");
		return EXIT_FAILURE;
	}

	if (setsignal(SIGPIPE, SIG_IGN, 0)
#if !EV_CHILD_ENABLE
	    || setsignal(SIGCHLD, SIG_DFL, SA_NOCLDSTOP | SA_NOCLDWAIT)
#endif
	   )
		goto error;

	/* create main Lua state */
	L = luaL_newstate();
	if (L == NULL) {
		lem_log_error("lem: error initializing Lua state");
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

	/* initialize threadpool */
	if (pool_init()) {
		lem_log_error("lem: error initializing threadpool");
		goto error;
	}

	/* load file */
	if (queue_file(argc, argv, 1))
		goto error;

	/* start the mainloop */
	ev_loop(LEM_ 0);
	lem_debug("event loop exited");

	/* if there is an error message left on L print it */
	if (lua_type(L, -1) == LUA_TSTRING)
		lem_log_error("lem: %s", lua_tostring(L, -1));

	/* shutdown Lua */
	lua_close(L);

	/* free runqueue */
	free(rq.queue);

	/* destroy loop */
#if EV_MULTIPLICITY
	ev_loop_destroy(lem_loop);
#else
	ev_default_destroy();
#endif
	lem_debug("Bye %s", exit_status == EXIT_SUCCESS ? "o/" : ":(");
	return exit_status;

error:
	if (L)
		lua_close(L);
	if (rq.queue)
		free(rq.queue);
#if EV_MULTIPLICITY
	ev_loop_destroy(lem_loop);
#else
	ev_default_destroy();
#endif
	return EXIT_FAILURE;
}
