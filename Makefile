CC           = gcc
CFLAGS      ?= -O2 -pipe -Wall -Wextra -Wno-variadic-macros -Wno-strict-aliasing
PKGCONFIG    = pkg-config
STRIP        = strip
INSTALL      = install
SED          = sed
UNAME        = uname

headers      = lem.h config.h macros.h libev/ev.h
programs     = lem utils.so
scripts      = repl.lua lem-repl

objects      = event.o lem.o

OS           = $(shell $(UNAME))
LUA          = embedded
LUA_VERSION  = 5.1
DESTDIR      =
PREFIX       = /usr/local
BINDIR       = $(PREFIX)/bin
INCDIR       = $(PREFIX)/include
PKG_CONFIG_PATH = $(PREFIX)/lib/pkgconfig

ifeq ($(OS),Darwin)
SHARED       = -dynamiclib -Wl,-undefined,dynamic_lookup
STRIP_ARGS   = -u -r
else
SHARED       = -shared
endif

ifeq ($(LUA),embedded)
CFLAGS      += -Ilua -DLUA_USE_LINUX -DLUA_ROOT='"$(PREFIX)/"'
LIBRARIES    = -lm

ifeq ($(OS), Linux)
LIBRARIES   += -ldl
endif

headers     += lua/luaconf.h lua/lua.h lua/lauxlib.h
# From lua/Makefile
CORE_O       = lapi.o lcode.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o lmem.o \
               lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o  \
               lundump.o lvm.o lzio.o
LIB_O        = lauxlib.o lbaselib.o ldblib.o liolib.o lmathlib.o loslib.o ltablib.o \
               lstrlib.o loadlib.o linit.o
objects     += $(CORE_O:%=lua/%) $(LIB_O:%=lua/%)

LUA_PATH     = $(PREFIX)/share/lua/$(LUA_VERSION)
LUA_CPATH    = $(PREFIX)/lib/lua/$(LUA_VERSION)
LIB_INCLUDES = -I$(INCDIR) -I$(INCDIR)/lem
else
LUA_PATH     = $(shell $(LUA) -e 'print(package.path:match("([^;]*/lua/$(LUA_VERSION))"))')
LUA_CPATH    = $(shell $(LUA) -e 'print(package.cpath:match("([^;]*/lua/$(LUA_VERSION))"))')

ifeq ($(findstring LuaJIT, $(shell $(LUA) -v 2>&1)),)
LIBRARIES    = -llua
LUA_INCDIR   = $(INCDIR)
LIB_INCLUDES = -I$(LUA_INCDIR)
else
CFLAGS      += $(shell $(PKGCONFIG) --cflags luajit)
LIBRARIES    = $(shell $(PKGCONFIG) --libs luajit)
LIB_INCLUDES = -I$(INCDIR) $(shell $(PKGCONFIG) --cflags-only-I luajit)
endif
endif

ifdef NDEBUG
CFLAGS += -DNDEBUG
endif

ifdef V
M=@\#
O=
else
M=@
O=@
endif

.PHONY: all strip install clean
.PRECIOUS: %.pic.o

all: $(programs)

config.h: config.$(OS)
	$Mecho '  CP $@'
	$Ocp $< $@

lem.pc: lem.pc.in
	$Mecho '  SED $@'
	$O$(SED) -e 's|@PATH@|$(LUA_PATH)|;s|@CPATH@|$(LUA_CPATH)|;s|@LIB_INCLUDES@|$(LIB_INCLUDES)|' $< > $@

%.pic.o: %.c config.h
	$Mecho '  CC $@'
	$O$(CC) $(CFLAGS) -Iinclude -fPIC -nostartfiles -c $< -o $@

event.o: CFLAGS += -w

%.o: %.c config.h
	$Mecho '  CC $@'
	$O$(CC) $(CFLAGS) -Iinclude -c $< -o $@

lem: $(objects)
	$Mecho '  LD $@'
	$O$(CC) -rdynamic $(LIBRARIES) $(LDFLAGS) $^ -o $@

utils.so: utils.pic.o
	$Mecho '  LD $@'
	$O$(CC) $(SHARED) $(LDFLAGS) $^ -o $@

%-strip: %
	$Mecho '  STRIP $<'
	$O$(STRIP) $(STRIP_ARGS) $<

strip: $(programs:%=%-strip)

bindir-install:
	$Mecho "  INSTALL -d $(BINDIR)"
	$O$(INSTALL) -d $(DESTDIR)$(BINDIR)

lem-install: lem bindir-install
	$Mecho "  INSTALL $<"
	$O$(INSTALL) $< $(DESTDIR)$(BINDIR)/$<

lem-repl-install: lem-repl bindir-install
	$Mecho "  INSTALL $<"
	$O$(INSTALL) $< $(DESTDIR)$(BINDIR)/$<

incdir-install:
	$Mecho "  INSTALL -d $(INCDIR)/lem"
	$O$(INSTALL) -d $(DESTDIR)$(INCDIR)/lem

lem.h-install: lem.h incdir-install
	$Mecho "  INSTALL $<"
	$O$(INSTALL) -m644 $< $(DESTDIR)$(INCDIR)/$<

%.h-install: %.h incdir-install
	$Mecho "  INSTALL $(notdir $<)"
	$O$(INSTALL) -m644 $< $(DESTDIR)$(INCDIR)/lem/$(notdir $<)

path-install:
	$Mecho "  INSTALL -d $(LUA_PATH)"
	$O$(INSTALL) -d $(DESTDIR)$(LUA_PATH)/lem

%.lua-install: %.lua path-install
	$Mecho "  INSTALL $<"
	$O$(INSTALL) -m644 $< $(DESTDIR)$(LUA_PATH)/lem/$<

cpath-install:
	$Mecho "  INSTALL -d $(LUA_CPATH)"
	$O$(INSTALL) -d $(DESTDIR)$(LUA_CPATH)/lem

%.so-install: %.so cpath-install
	$Mecho "  INSTALL $<"
	$O$(INSTALL) $< $(DESTDIR)$(LUA_CPATH)/lem/$<

pkgdir-install:
	$Mecho "  INSTALL -d $(PKG_CONFIG_PATH)"
	$O$(INSTALL) -d $(DESTDIR)$(PKG_CONFIG_PATH)

%.pc-install: %.pc pkgdir-install
	$Mecho "  INSTALL $<"
	$O$(INSTALL) -m644 $< $(DESTDIR)$(PKG_CONFIG_PATH)/$<

install: lem.pc-install $(headers:%=%-install) $(programs:%=%-install) $(scripts:%=%-install)

clean:
	rm -f config.h lem.pc $(programs) *.o lua/*.o *.c~ *.h~
