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

#if !(LUA_VERSION_NUM >= 502)
#define lua_getuservalue lua_getfenv
#define lua_setuservalue lua_setfenv
#endif

enum classes {
	C_CTL,   /* control characters */
	C_LF,    /* \n */
	C_CR,    /* \r */
	C_LWS,   /* space or \t */
	C_TSPCL, /* tspecials */
	C_SLASH, /* / */
	C_COLON, /* : */
	C_DOT,   /* . */
	C_NUM,   /* 0-9 */
	C_H,     /* H */
	C_T,     /* T */
	C_P,     /* P */
	C_ETC,   /* the rest */
	C_MAX
};

/*
 * This array maps the first 128 ASCII characters into character classes
 * The remaining characters should be mapped to C_ETC
 */
static const unsigned char ascii_class[128] = {
	C_CTL,   C_CTL,   C_CTL,   C_CTL,   C_CTL,   C_CTL,   C_CTL,   C_CTL,
	C_CTL,   C_LWS,   C_LF,    C_CTL,   C_CTL,   C_CR,    C_CTL,   C_CTL,
	C_CTL,   C_CTL,   C_CTL,   C_CTL,   C_CTL,   C_CTL,   C_CTL,   C_CTL,
	C_CTL,   C_CTL,   C_CTL,   C_CTL,   C_CTL,   C_CTL,   C_CTL,   C_CTL,

	C_LWS,   C_ETC,   C_TSPCL, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_TSPCL, C_TSPCL, C_ETC,   C_ETC,   C_TSPCL, C_ETC,   C_DOT,   C_SLASH,
	C_NUM,   C_NUM,   C_NUM,   C_NUM,   C_NUM,   C_NUM,   C_NUM,   C_NUM,
	C_NUM,   C_NUM,   C_COLON, C_TSPCL, C_TSPCL, C_TSPCL, C_TSPCL, C_TSPCL,

	C_TSPCL, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_H,     C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_P,     C_ETC,   C_ETC,   C_ETC,   C_T,     C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_TSPCL, C_TSPCL, C_TSPCL, C_ETC,   C_ETC,

	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_H,     C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_P,     C_ETC,   C_ETC,   C_ETC,   C_T,     C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_TSPCL, C_ETC,   C_TSPCL, C_ETC,   C_CTL
};

enum states {
	S_GO,
	SMTD,
	SMUS,
	SURI,
	SUHS,
	SH__,
	SHT1,
	SHT2,
	SHTP,
	SSLH,
	SMAV,
	SDOT,
	SMIV,
	C_GO,
	CH__,
	CHT1,
	CHT2,
	CHTP,
	CSLH,
	CMAV,
	CDOT,
	CMIV,
	CVNS,
	CNUM,
	CNTS,
	CTXT,
	SRE1,
	SRE2,
	SKEY,
	SCOL,
	SVAL,
	SLWS,
	SCN1,
	SCN2,
	SNL1,
	SNL2,
	SEND,
	SMAX,
	X___ = SMAX,
	XMUS,
	XUHS,
	XVNS,
	XNTS,
	XRE1,
	XKEY,
	XCOL,
	XVAL,
	XEND
};

static const unsigned char state_table[SMAX][C_MAX] = {
/*           ctl  \n   \r   lws  tsp  /    :    .   num   H    T    P    etc */
/* S_GO*/ { X___,X___,X___,X___,X___,X___,X___,SMTD,SMTD,SMTD,SMTD,SMTD,SMTD },
/* SMTD*/ { X___,X___,X___,XMUS,X___,X___,X___,SMTD,SMTD,SMTD,SMTD,SMTD,SMTD },
/* SMUS*/ { X___,X___,X___,SMUS,SURI,SURI,SURI,SURI,SURI,SURI,SURI,SURI,SURI },
/* SURI*/ { X___,X___,X___,XUHS,SURI,SURI,SURI,SURI,SURI,SURI,SURI,SURI,SURI },
/* SUHS*/ { X___,X___,X___,SUHS,X___,X___,X___,X___,X___,SH__,X___,X___,X___ },
/* SH__*/ { X___,X___,X___,X___,X___,X___,X___,X___,X___,X___,SHT1,X___,X___ },
/* SHT1*/ { X___,X___,X___,X___,X___,X___,X___,X___,X___,X___,SHT2,X___,X___ },
/* SHT2*/ { X___,X___,X___,X___,X___,X___,X___,X___,X___,X___,X___,SHTP,X___ },
/* SHTP*/ { X___,X___,X___,SHTP,X___,SSLH,X___,X___,X___,X___,X___,X___,X___ },
/* SSLH*/ { X___,X___,X___,SSLH,X___,X___,X___,X___,SMAV,X___,X___,X___,X___ },
/* SMAV*/ { X___,X___,X___,X___,X___,X___,X___,SDOT,SMAV,X___,X___,X___,X___ },
/* SDOT*/ { X___,X___,X___,X___,X___,X___,X___,X___,SMIV,X___,X___,X___,X___ },
/* SMIV*/ { X___,X___,SRE1,X___,X___,X___,X___,X___,SMIV,X___,X___,X___,X___ },
/* C_GO*/ { X___,X___,X___,X___,X___,X___,X___,X___,X___,CH__,X___,X___,X___ },
/* CH__*/ { X___,X___,X___,X___,X___,X___,X___,X___,X___,X___,CHT1,X___,X___ },
/* CHT1*/ { X___,X___,X___,X___,X___,X___,X___,X___,X___,X___,CHT2,X___,X___ },
/* CHT2*/ { X___,X___,X___,X___,X___,X___,X___,X___,X___,X___,X___,CHTP,X___ },
/* CHTP*/ { X___,X___,X___,CHTP,X___,CSLH,X___,X___,X___,X___,X___,X___,X___ },
/* CSLH*/ { X___,X___,X___,CSLH,X___,X___,X___,X___,CMAV,X___,X___,X___,X___ },
/* CMAV*/ { X___,X___,X___,X___,X___,X___,X___,CDOT,CMAV,X___,X___,X___,X___ },
/* CDOT*/ { X___,X___,X___,X___,X___,X___,X___,X___,CMIV,X___,X___,X___,X___ },
/* CMIV*/ { X___,X___,X___,XVNS,X___,X___,X___,X___,CMIV,X___,X___,X___,X___ },
/* CVNS*/ { X___,X___,X___,CVNS,X___,X___,X___,X___,CNUM,X___,X___,X___,X___ },
/* CNUM*/ { X___,X___,X___,XNTS,X___,X___,X___,X___,CNUM,X___,X___,X___,X___ },
/* CNTS*/ { X___,X___,X___,CNTS,CTXT,X___,X___,X___,CTXT,CTXT,CTXT,CTXT,CTXT },
/* CTXT*/ { X___,X___,XRE1,CTXT,CTXT,X___,X___,X___,CTXT,CTXT,CTXT,CTXT,CTXT },
/* SRE1*/ { X___,SRE2,X___,X___,X___,X___,X___,X___,X___,X___,X___,X___,X___ },
/* SRE2*/ { X___,X___,SEND,X___,X___,X___,X___,SKEY,SKEY,SKEY,SKEY,SKEY,SKEY },
/* SKEY*/ { X___,X___,X___,X___,X___,X___,XCOL,SKEY,SKEY,SKEY,SKEY,SKEY,SKEY },
/* SCOL*/ { X___,X___,SCN1,SCOL,SVAL,SVAL,SVAL,SVAL,SVAL,SVAL,SVAL,SVAL,SVAL },
/* SVAL*/ { X___,X___,SNL1,SLWS,SVAL,SVAL,SVAL,SVAL,SVAL,SVAL,SVAL,SVAL,SVAL },
/* SLWS*/ { X___,X___,SNL1,SLWS,XVAL,XVAL,XVAL,XVAL,XVAL,XVAL,XVAL,XVAL,XVAL },
/* SCN1*/ { X___,SCN2,X___,X___,X___,X___,X___,X___,X___,X___,X___,X___,X___ },
/* SCN2*/ { X___,X___,SEND,SCOL,X___,X___,X___,XKEY,XKEY,XKEY,XKEY,XKEY,XKEY },
/* SNL1*/ { X___,SNL2,X___,X___,X___,X___,X___,X___,X___,X___,X___,X___,X___ },
/* SNL2*/ { X___,X___,SEND,SLWS,X___,X___,X___,XKEY,XKEY,XKEY,XKEY,XKEY,XKEY },
/* SEND*/ { X___,XEND,X___,X___,X___,X___,X___,X___,X___,X___,X___,X___,X___ },
};

static void
parse_http_init(lua_State *T)
{
	/* create result table */
	lua_settop(T, 2);
	lua_createtable(T, 0, 5);
	lua_pushvalue(T, 1);
	lua_setfield(T, -2, "client");
}

static void
parse_http_req_init(lua_State *T, struct lem_inputbuf *b)
{
	b->u = S_GO;
	parse_http_init(T);
}

static void
parse_http_res_init(lua_State *T, struct lem_inputbuf *b)
{
	b->u = C_GO;
	parse_http_init(T);
}

static int
parse_http_process(lua_State *T, struct lem_inputbuf *b)
{
	unsigned char state = b->u & 0xFF;
	unsigned int w = b->u >> 8;
	unsigned int r = b->start;
	unsigned int end = b->end;

	while (r < end) {
		unsigned char ch = b->buf[r++];

		state = state_table[state][ch > 127 ? C_ETC : ascii_class[ch]];
		/*lem_debug("char = %c (%hhu), state = %hhu", ch, ch, state);*/
		switch (state) {
		case SMTD:
		case SURI:
		case SMAV:
		case SDOT:
		case SMIV:
		case CMAV:
		case CDOT:
		case CMIV:
		case CNUM:
		case CTXT:
		case SVAL:
			b->buf[w++] = ch;
			break;

		case XKEY:
			state = SKEY;
			lua_pushlstring(T, b->buf, w);
			lua_rawset(T, -3);
			w = 0;
			/* fallthrough */

		case SKEY:
			if (ch >= 'A' && ch <= 'Z')
				ch += ('a' - 'A');
			b->buf[w++] = ch;
			break;

		case SRE1:
			lua_pushlstring(T, b->buf, w);
			lua_setfield(T, -2, "version");
			w = 0;
			lua_newtable(T);
			break;

		case X___:
			lem_debug("HTTP parse error");
			lua_settop(T, 0);
			lua_pushnil(T);
			lua_pushliteral(T, "parse error");
			return 2;

		case XMUS:
			state = SMUS;
			lua_pushlstring(T, b->buf, w);
			lua_setfield(T, -2, "method");
			w = 0;
			break;

		case XUHS:
			state = SUHS;
			lua_pushlstring(T, b->buf, w);
			lua_setfield(T, -2, "uri");
			w = 0;
			break;

		case XVNS:
			state = CVNS;
			lua_pushlstring(T, b->buf, w);
			lua_setfield(T, -2, "version");
			w = 0;
			break;

		case XNTS:
			state = CNTS;
			{
				unsigned int n = 0;
				unsigned int k;

				for (k = 0; k < w; k++) {
					n *= 10;
					n += b->buf[k] - '0';
				}

				lua_pushnumber(T, n);
			}
			lua_setfield(T, -2, "status");
			w = 0;
			break;

		case XRE1:
			state = SRE1;
			lua_pushlstring(T, b->buf, w);
			lua_setfield(T, -2, "text");
			w = 0;
			lua_newtable(T);
			break;

		case XCOL:
			state = SCOL;
			lua_pushlstring(T, b->buf, w);
			w = 0;
			break;

		case XVAL:
			state = SVAL;
			b->buf[w++] = ' ';
			b->buf[w++] = ch;
			break;

		case XEND:
			/* in case there are no headers this is false */
			if (lua_type(T, -1) == LUA_TSTRING) {
				lua_pushlstring(T, b->buf, w);
				lua_rawset(T, -3);
			}
			lua_setfield(T, -2, "headers");

			/* set metatable */
			lua_getuservalue(T, 2);
			lua_setmetatable(T, -2);

			if (r == end)
				b->start = b->end = 0;
			else
				b->start = r;
			return 1;
		}
	}

	if (w == LEM_INPUTBUF_SIZE - 1) {
		b->start = b->end = 0;
		lua_settop(T, 0);
		lua_pushnil(T);
		lua_pushliteral(T, "out of buffer space");
		return 2;
	}

	b->start = b->end = w + 1;
	b->u = (w << 8) | state;
	return LEM_PMORE;
}

int
luaopen_lem_http_core(lua_State *L)
{
	struct lem_parser *p;

	/* create module table M */
	lua_newtable(L);

	/* create Request metatable */
	lua_newtable(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	/* insert Request metatable */
	lua_setfield(L, -2, "Request");

	/* create Response metatable */
	lua_newtable(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	/* insert Request metatable */
	lua_setfield(L, -2, "Response");

	p = lua_newuserdata(L, sizeof(struct lem_parser));
	p->init = parse_http_req_init;
	p->process = parse_http_process;
	p->destroy = NULL;
	lua_getfield(L, -2, "Request");
	lua_setuservalue(L, -2);
	lua_setfield(L, -2, "HTTPRequest");

	p = lua_newuserdata(L, sizeof(struct lem_parser));
	p->init = parse_http_res_init;
	p->process = parse_http_process;
	p->destroy = NULL;
	lua_getfield(L, -2, "Response");
	lua_setuservalue(L, -2);
	lua_setfield(L, -2, "HTTPResponse");

	return 1;
}
