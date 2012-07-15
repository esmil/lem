/* setup for luaconf.h */
#define LUA_CORE
#define LUA_LIB
#define ltable_c
#define lvm_c
#include <luaconf.h>

/* do not export internal symbols */
#undef LUAI_FUNC
#undef LUAI_DDEC
#undef LUAI_DDEF
#define LUAI_FUNC	static
#define LUAI_DDEC	static
#define LUAI_DDEF	static

/* core */
#include "../lua/lapi.c"
#include "../lua/lcode.c"
#include "../lua/lctype.c"
#include "../lua/ldebug.c"
#include "../lua/ldo.c"
#include "../lua/ldump.c"
#include "../lua/lfunc.c"
#include "../lua/lgc.c"
#include "../lua/llex.c"
#include "../lua/lmem.c"
#include "../lua/lobject.c"
#include "../lua/lopcodes.c"
#include "../lua/lparser.c"
#include "../lua/lstate.c"
#include "../lua/lstring.c"
#include "../lua/ltable.c"
#include "../lua/ltm.c"
#include "../lua/lundump.c"
#include "../lua/lvm.c"
#include "../lua/lzio.c"

/* auxiliary library */
#include "../lua/lauxlib.c"

/* standard library */
#include "../lua/lbaselib.c"
#include "../lua/lbitlib.c"
#include "../lua/lcorolib.c"
#include "../lua/ldblib.c"
#include "../lua/liolib.c"
#include "../lua/lmathlib.c"
#include "../lua/loadlib.c"
#include "../lua/loslib.c"
#include "../lua/lstrlib.c"
#include "../lua/ltablib.c"
#include "../lua/linit.c"
