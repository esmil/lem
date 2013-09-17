/*
 * This file is part of LEM, a Lua Event Machine.
 * Copyright 2013 Asbjørn Sloth Tønnesen
 * Copyright 2013 Emil Renner Berthing
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

#include <lem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct signal_mapping {
	const char *name;
	uint8_t	no;
};

/* Several signal numbers are architecture-dependent,
 * therefore we need a lookup table on the C-side */
#define ARRAYLEN(a) (sizeof(a)/sizeof((a)[0]))
#define _(sig) { #sig, SIG ## sig }
static struct signal_mapping sigmap[] = {
	_(HUP), _(INT), _(USR1), _(USR2),
	_(QUIT), _(ILL), _(TRAP), _(ABRT),
	_(BUS), _(FPE), _(SEGV), _(PIPE),
	_(ALRM), _(TERM), _(CONT), _(CHLD),
	_(TSTP), _(TTIN), _(TTOU), _(PWR),
	_(PROF), _(SYS), _(URG), _(VTALRM),
	_(XCPU), _(XFSZ), _(WINCH)
};
#undef _

#if EV_SIGNAL_ENABLE
struct sigwatcher {
	struct sigwatcher *next;
	struct ev_signal w;
};

static sigset_t signal_sigset;
static struct sigwatcher *signal_watchers;

static void
signal_os_handler(EV_P_ struct ev_signal *w, int revents)
{
	lua_State *S;

	(void)revents;

	S = lem_newthread();
	lua_pushlightuserdata(S, &sigmap);
	lua_rawget(S, LUA_REGISTRYINDEX);
	if (lua_type(S, 1) != LUA_TFUNCTION) {
		lem_forgetthread(S);
		return;
	}

	lua_pushinteger(S, w->signum);
	lem_queue(S, 1);
}

static int
signal_os_watch(lua_State *T, int sig)
{
	struct sigwatcher *s;

	if (sigismember(&signal_sigset, sig))
		goto out; /* already watched */

	s = lem_xmalloc(sizeof(struct sigwatcher));

	ev_signal_init(&s->w, signal_os_handler, sig);
	ev_set_priority(&s->w, EV_MAXPRI);
	ev_signal_start(LEM_ &s->w);
	ev_unref(LEM); /* watcher shouldn't keep loop alive */

	sigaddset(&signal_sigset, sig);
	pthread_sigmask(SIG_UNBLOCK, &signal_sigset, NULL);

	s->next = signal_watchers;
	signal_watchers = s;
out:
	lua_pushboolean(T, 1);
	return 1;
}

static int
signal_os_unwatch(lua_State *T, int sig)
{
	struct sigwatcher **prevp;
	struct sigwatcher *s;

	for (prevp = &signal_watchers, s = signal_watchers;
			s != NULL;
			prevp = &s->next, s = s->next) {
		if (s->w.signum == sig)
			break;
	}
	if (s != NULL) {
		ev_ref(LEM);
		ev_signal_stop(LEM_ &s->w);

		sigdelset(&signal_sigset, sig);

		*prevp = s->next;
		free(s);
	}
	lua_pushboolean(T, 1);
	return 1;
}
#else /* EV_SIGNAL_ENABLE */
static int
signal_os_unsupported(lua_State *T)
{
	lua_pushnil(T);
	lua_pushliteral(T, "Your libev is compiled without signal support.");
	return 2
}

static inline int
signal_os_watch(lua_State *T)
{
	return signal_os_unsupported(T);
}

static inline int
signal_os_unwatch(lua_State *T)
{
	return signal_os_unsupported(T);
}
#endif

#if EV_CHILD_ENABLE
static struct ev_child signal_child_watcher;

static void
signal_child_handler(EV_P_ struct ev_child *w, int revents)
{
	lua_State *S;
	int status;

	(void)revents;

	S = lem_newthread();
	lua_pushlightuserdata(S, &sigmap);
	lua_rawget(S, LUA_REGISTRYINDEX);
	if (lua_type(S, 1) != LUA_TFUNCTION) {
		lem_forgetthread(S);
		return;
	}
	lua_pushinteger(S, SIGCHLD);

	status = w->rstatus;
	lua_createtable(S, 0, 3);

	if (WIFEXITED(status)) {
		lua_pushinteger(S, WEXITSTATUS(status));
		lua_setfield(S, -2, "status");
		lua_pushstring(S, "exited");
	} else if (WIFSIGNALED(status)) {
		lua_pushinteger(S, WTERMSIG(status));
		lua_setfield(S, -2, "signal");
#ifdef WCOREDUMP
		lua_pushboolean(S, WCOREDUMP(status));
		lua_setfield(S, -2, "coredumped");
#endif
		lua_pushstring(S, "signaled");
	} else if (WIFSTOPPED(status)) {
		lua_pushinteger(S, WSTOPSIG(status));
		lua_setfield(S, -2, "signal");
		lua_pushstring(S, "stopped");
	} else if (WIFCONTINUED(status)) {
		lua_pushstring(S, "continued");
	} else {
		assert(0); /* XXX do something more graceful */
	}
	lua_setfield(S, -2, "type");

	lem_queue(S, 2);
}

static int
signal_child_watch(lua_State *T)
{
	if (!ev_is_active(&signal_child_watcher)) {
		ev_child_start(LEM_ &signal_child_watcher);
		ev_unref(LEM); /* watcher shouldn't keep loop alive */
	}
	lua_pushboolean(T, 1);
	return 1;
}

static int
signal_child_unwatch(lua_State *T)
{
	if (ev_is_active(&signal_child_watcher)) {
		ev_ref(LEM);
		ev_child_stop(LEM_ &signal_child_watcher);
	}
	lua_pushboolean(T, 1);
	return 1;
}
#else /* EV_CHILD_ENABLE */
static int
signal_child_unsupported(lua_State *T)
{
	lua_pushnil(T);
	lua_pushliteral(T, "Your libev is compiled without child support.");
	return 2
}

static inline int
signal_child_watch(lua_State *T)
{
	return signal_child_unsupported(T);
}

static inline int
signal_child_unwatch(lua_State *T)
{
	return signal_child_unsupported(T);
}
#endif

static int
signal_lookup(lua_State *T)
{
	const char *needle = luaL_checkstring(T, 1);

	unsigned int i;

	for (i = 0; i < ARRAYLEN(sigmap); i++) {
		struct signal_mapping *sig = &sigmap[i];
		if (strcmp(sig->name, needle) == 0) {
			lua_pushinteger(T, sig->no);
			return 1;
		}
	}
	lua_pushnil(T);
	return 1;
}

static int
signal_sethandler(lua_State *T)
{
	int type;

	if (lua_gettop(T) < 1)
		lua_pushnil(T);

	type = lua_type(T, 1);
	if (type != LUA_TNIL && type != LUA_TFUNCTION)
		return luaL_argerror(T, 1, "expected nil or a function");

	lua_settop(T, 1);
	lua_pushlightuserdata(T, &sigmap);
	lua_insert(T, 1);
	lua_rawset(T, LUA_REGISTRYINDEX);
	return 0;
}

static int
signal_watch(lua_State *T)
{
	int sig = luaL_checkint(T, 1);

	lua_settop(T, 1);
	lua_pushlightuserdata(T, &sigmap);
	lua_rawget(T, LUA_REGISTRYINDEX);
	if (lua_isnil(T, 2))
		return luaL_error(T, "You must set a signal handler first");

	if (sig == SIGCHLD)
		return signal_child_watch(T);

	return signal_os_watch(T, sig);
}

static int
signal_unwatch(lua_State *T)
{
	int sig = luaL_checkint(T, 1);

	if (sig == SIGCHLD)
		return signal_child_unwatch(T);

	return signal_os_unwatch(T, sig);
}

int
luaopen_lem_signal_core(lua_State *T)
{
#if EV_CHILD_ENABLE
	ev_child_init(&signal_child_watcher, signal_child_handler, 0, 1);
#endif

#if EV_SIGNAL_ENABLE
	sigemptyset(&signal_sigset);
	/* signal_watchers = NULL; globals are zero initalized */
#endif

	/* create module table */
	lua_newtable(T);

	/* set lookup function */
	lua_pushcfunction(T, signal_lookup);
	lua_setfield(T, -2, "lookup");
	/* set sethandler function */
	lua_pushcfunction(T, signal_sethandler);
	lua_setfield(T, -2, "sethandler");
	/* set watch function */
	lua_pushcfunction(T, signal_watch);
	lua_setfield(T, -2, "watch");
	/* set unwatch function */
	lua_pushcfunction(T, signal_unwatch);
	lua_setfield(T, -2, "unwatch");

	return 1;
}
