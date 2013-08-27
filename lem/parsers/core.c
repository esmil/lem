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

#include <lem-parsers.h>

#define LEM_PSTATE_CHECK(x) LEM_BUILD_ASSERT(sizeof(x) < LEM_INPUTBUF_PSIZE)

/*
 * read available data
 */
static int
parse_available_process(lua_State *T, struct lem_inputbuf *b)
{
	unsigned int size = b->end - b->start;

	if (size == 0)
		return 0;

	lua_pushlstring(T, b->buf + b->start, size);
	b->start = b->end = 0;
	return 1;
}

static const struct lem_parser parser_available = {
	.process = parse_available_process,
};

/*
 * read a specified number of bytes
 */
struct parse_target_state {
	size_t target;
	int parts;
};
LEM_PSTATE_CHECK(struct parse_target_state);

static void
parse_target_init(lua_State *T, struct lem_inputbuf *b)
{
	struct parse_target_state *s = (struct parse_target_state *)&b->pstate;

	s->target = luaL_checknumber(T, 3);
	s->parts = 0;
	lua_settop(T, 2);
}

static int
parse_target_process(lua_State *T, struct lem_inputbuf *b)
{
	struct parse_target_state *s = (struct parse_target_state *)&b->pstate;
	size_t size = b->end - b->start;

	if (size >= s->target) {
		lua_pushlstring(T, b->buf + b->start, s->target);
		lua_concat(T, s->parts + 1);
		b->start += s->target;
		if (b->start == b->end)
			b->start = b->end = 0;

		return 1;
	}

	if (b->end == LEM_INPUTBUF_SIZE) {
		lua_pushlstring(T, b->buf + b->start, size);
		s->parts++;
		if (s->parts == LUA_MINSTACK-2) {
			lua_concat(T, LUA_MINSTACK-2);
			s->parts = 1;
		}
		b->start = b->end = 0;
		s->target -= size;
	}

	return 0;
}

static const struct lem_parser parser_target = {
	.init    = parse_target_init,
	.process = parse_target_process,
};

/*
 * read all data until stream closes
 */
struct parse_all_state {
	int parts;
};
LEM_PSTATE_CHECK(struct parse_all_state);

static void
parse_all_init(lua_State *T, struct lem_inputbuf *b)
{
	struct parse_all_state *s = (struct parse_all_state *)&b->pstate;

	s->parts = 0;
	lua_settop(T, 2);
}

static int
parse_all_process(lua_State *T, struct lem_inputbuf *b)
{
	struct parse_all_state *s = (struct parse_all_state *)&b->pstate;

	if (b->end == LEM_INPUTBUF_SIZE) {
		lua_pushlstring(T, b->buf + b->start,
				LEM_INPUTBUF_SIZE - b->start);
		s->parts++;
		if (s->parts == LUA_MINSTACK-2) {
			lua_concat(T, LUA_MINSTACK-2);
			s->parts = 1;
		}
		b->start = b->end = 0;
	}

	return 0;
}

static int
parse_all_destroy(lua_State *T, struct lem_inputbuf *b, enum lem_preason reason)
{
	struct parse_all_state *s = (struct parse_all_state *)&b->pstate;
	unsigned int size;

	if (reason != LEM_PCLOSED)
		return 0;

	size = b->end - b->start;
	if (size > 0) {
		lua_pushlstring(T, b->buf + b->start, size);
		s->parts++;
	}
	b->start = b->end = 0;

	lua_concat(T, s->parts);
	return 1;
}

static const struct lem_parser parser_all = {
	.init    = parse_all_init,
	.process = parse_all_process,
	.destroy = parse_all_destroy,
};

/*
 * read a line
 */
struct parse_line_state {
	int parts;
	char stopbyte;
};
LEM_PSTATE_CHECK(struct parse_line_state);

static void
parse_line_init(lua_State *T, struct lem_inputbuf *b)
{
	struct parse_line_state *s = (struct parse_line_state *)&b->pstate;
	const char *stopbyte = luaL_optstring(T, 3, "\n");

	s->parts = 0;
	s->stopbyte = stopbyte[0];
	lua_settop(T, 2);
}

static int
parse_line_process(lua_State *T, struct lem_inputbuf *b)
{
	struct parse_line_state *s = (struct parse_line_state *)&b->pstate;
	unsigned int i;

	for (i = b->start; i < b->end; i++) {
		if (b->buf[i] == s->stopbyte) {
			lua_pushlstring(T, b->buf + b->start, i - b->start);
			lua_concat(T, s->parts + 1);
			i++;
			if (i == b->end)
				b->start = b->end = 0;
			else
				b->start = i;
			return 1;
		}
	}

	if (b->end == LEM_INPUTBUF_SIZE) {
		lua_pushlstring(T, b->buf + b->start,
				LEM_INPUTBUF_SIZE - b->start);
		s->parts++;
		if (s->parts == LUA_MINSTACK-2) {
			lua_concat(T, LUA_MINSTACK-2);
			s->parts = 1;
		}
		b->start = b->end = 0;
	}

	return 0;
}

static const struct lem_parser parser_line = {
	.init    = parse_line_init,
	.process = parse_line_process,
};

int
luaopen_lem_parsers_core(lua_State *L)
{
	/* create module table */
	lua_newtable(L);

	/* create lookup table */
	lua_createtable(L, 0, 4);
	/* push parser_line */
	lua_pushlightuserdata(L, (void *)&parser_available);
	lua_setfield(L, -2, "available");
	/* push parser_target */
	lua_pushlightuserdata(L, (void *)&parser_target);
	lua_setfield(L, -2, "target");
	/* push parser_all */
	lua_pushlightuserdata(L, (void *)&parser_all);
	lua_setfield(L, -2, "*a");
	/* push parser_line */
	lua_pushlightuserdata(L, (void *)&parser_line);
	lua_setfield(L, -2, "*l");
	/* insert lookup table */
	lua_setfield(L, -2, "lookup");

	return 1;
}
