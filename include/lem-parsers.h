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

#ifndef _LEM_PARSERS_H
#define _LEM_PARSERS_H

#include <lem.h>

#define LEM_INPUTBUF_SIZE 4096

struct lem_inputbuf {
	unsigned int start;
	unsigned int end;
	union {
		void *p;
		unsigned long u;
	};
	int parts;
	char buf[LEM_INPUTBUF_SIZE];
};

enum lem_presult {
	LEM_PMORE = 0,
};

enum lem_preason {
	LEM_PCLOSED,
	LEM_PERROR,
};

struct lem_parser {
	void (*init)(lua_State *T, struct lem_inputbuf *b);
	int (*process)(lua_State *T, struct lem_inputbuf *b);
	int (*destroy)(lua_State *T, struct lem_inputbuf *b, enum lem_preason reason);
};

static inline void
lem_inputbuf_init(struct lem_inputbuf *buf)
{
	buf->start = buf->end = 0;
}

#endif
