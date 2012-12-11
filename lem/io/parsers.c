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

/*
 * read available data
 */
static int
parse_available_process(lua_State *T, struct lem_inputbuf *b)
{
	size_t size = b->end - b->start;

	if (size == 0)
		return LEM_PMORE;

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
static void
parse_target_init(lua_State *T, struct lem_inputbuf *b)
{
	b->u = luaL_checknumber(T, 3);
	b->parts = 1;
	lua_settop(T, 2);
}

static int
parse_target_process(lua_State *T, struct lem_inputbuf *b)
{
	unsigned int target = b->u;
	unsigned int size = b->end - b->start;

	if (size >= target) {
		lua_pushlstring(T, b->buf + b->start, target);
		lua_concat(T, b->parts);
		b->start += target;
		if (b->start == b->end)
			b->start = b->end = 0;

		return 1;
	}

	if (b->end == LEM_INPUTBUF_SIZE) {
		lua_pushlstring(T, b->buf + b->start, size);
		b->parts++;
		b->start = b->end = 0;
		b->u = target - size;
	}

	return LEM_PMORE;
}

static const struct lem_parser parser_target = {
	.init    = parse_target_init,
	.process = parse_target_process,
};

/*
 * read all data until stream closes
 */
static void
parse_all_init(lua_State *T, struct lem_inputbuf *b)
{
	b->parts = 0;
	lua_settop(T, 2);
}

static int
parse_all_process(lua_State *T, struct lem_inputbuf *b)
{
	if (b->end == LEM_INPUTBUF_SIZE) {
		lua_pushlstring(T, b->buf + b->start,
				LEM_INPUTBUF_SIZE - b->start);
		b->start = b->end = 0;
		b->parts++;
	}

	return LEM_PMORE;
}

static int
parse_all_destroy(lua_State *T, struct lem_inputbuf *b, enum lem_preason reason)
{
	unsigned int size;

	if (reason != LEM_PCLOSED)
		return LEM_PMORE;

	size = b->end - b->start;
	if (size > 0) {
		lua_pushlstring(T, b->buf + b->start, size);
		b->start = b->end = 0;
		b->parts++;
	}

	lua_concat(T, b->parts);
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
static void
parse_line_init(lua_State *T, struct lem_inputbuf *b)
{
	const char *stopbyte = luaL_optstring(T, 3, "\n");

	b->u = (b->start << 8) | stopbyte[0];
	b->parts = 1;
	lua_settop(T, 2);
}

static int
parse_line_process(lua_State *T, struct lem_inputbuf *b)
{
	unsigned int i;
	unsigned char stopbyte = b->u & 0xFF;

	for (i = b->u >> 8; i < b->end; i++) {
		if (b->buf[i] == stopbyte) {
			lua_pushlstring(T, b->buf + b->start, i - b->start);
			lua_concat(T, b->parts);
			i++;
			if (i == b->end)
				b->start = b->end = 0;
			else
				b->start = i;
			return 1;
		}
	}

	if (b->end == LEM_INPUTBUF_SIZE) {
		lua_pushlstring(T, b->buf + b->start, b->end - b->start);
		b->parts++;
		b->start = b->end = 0;
		b->u = stopbyte;
	} else
		b->u = (i << 8) | stopbyte;

	return LEM_PMORE;
}

static const struct lem_parser parser_line = {
	.init    = parse_line_init,
	.process = parse_line_process,
};
