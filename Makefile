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

LIBRARIES    = -lm
ifeq ($(OS), Linux)
LIBRARIES   += -ldl
endif

ifeq ($(LUA),embedded)
CFLAGS      += -Ilua -DLUA_USE_LINUX -DLUA_ROOT='"$(PREFIX)/"'

headers     += lua/luaconf.h lua/lua.h lua/lauxlib.h
# From lua/Makefile
CORE_O       = lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o \
               lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o \
               ltm.o lundump.o lvm.o lzio.o
LIB_O        = lauxlib.o lbaselib.o lbitlib.o lcorolib.o ldblib.o liolib.o \
               lmathlib.o loslib.o lstrlib.o ltablib.o loadlib.o linit.o
objects     += $(CORE_O:%=lua/%) $(LIB_O:%=lua/%)

LUA_PATH     = $(PREFIX)/share/lua/5.2
LUA_CPATH    = $(PREFIX)/lib/lua/5.2
LIB_INCLUDES = -I$(INCDIR) -I$(INCDIR)/lem
else
LUA_PATH     = $(shell $(LUA) -e 'print(package.path:match("([^;]*)/%?"))')
LUA_CPATH    = $(shell $(LUA) -e 'print(package.cpath:match("([^;]*)/%?"))')

ifeq ($(findstring LuaJIT, $(shell $(LUA) -v 2>&1)),)
LIBRARIES   += -llua
LUA_INCDIR   = $(INCDIR)
LIB_INCLUDES = -I$(LUA_INCDIR)
else
CFLAGS      += $(shell $(PKGCONFIG) --cflags luajit)
LIBRARIES   += $(shell $(PKGCONFIG) --libs luajit)
LIB_INCLUDES = -I$(INCDIR) $(shell $(PKGCONFIG) --cflags-only-I luajit)
endif
endif

ifdef NDEBUG
CFLAGS += -DNDEBUG
endif

ifdef V
E=@\#
Q=
else
E=@echo
Q=@
endif

.PHONY: all strip install clean
.PRECIOUS: %.pic.o

all: $(programs)

config.h: config.$(OS)
	$E '  CP $@'
	$Qcp $< $@

lem.pc: lem.pc.in
	$E '  SED $@'
	$Q$(SED) -e 's|@PATH@|$(LUA_PATH)|;s|@CPATH@|$(LUA_CPATH)|;s|@LIB_INCLUDES@|$(LIB_INCLUDES)|' $< > $@

%.pic.o: %.c config.h
	$E '  CC $@'
	$Q$(CC) $(CFLAGS) -Iinclude -fPIC -nostartfiles -c $< -o $@

event.o: CFLAGS += -w
event.o: config.h

%.o: %.c config.h
	$E '  CC $@'
	$Q$(CC) $(CFLAGS) -Iinclude -c $< -o $@

lem: $(objects)
	$E '  LD $@'
	$Q$(CC) $^ -o $@ -rdynamic $(LDFLAGS) $(LIBRARIES)

utils.so: utils.pic.o
	$E '  LD $@'
	$Q$(CC) $(SHARED) $^ -o $@ $(LDFLAGS)

%-strip: %
	$E '  STRIP $<'
	$Q$(STRIP) $(STRIP_ARGS) $<

strip: $(programs:%=%-strip)

bindir-install:
	$E "  INSTALL -d $(BINDIR)"
	$Q$(INSTALL) -d $(DESTDIR)$(BINDIR)

lem-install: lem bindir-install
	$E "  INSTALL $<"
	$Q$(INSTALL) $< $(DESTDIR)$(BINDIR)/$<

lem-repl-install: lem-repl bindir-install
	$E "  INSTALL $<"
	$Q$(INSTALL) $< $(DESTDIR)$(BINDIR)/$<

incdir-install:
	$E "  INSTALL -d $(INCDIR)/lem"
	$Q$(INSTALL) -d $(DESTDIR)$(INCDIR)/lem

lem.h-install: lem.h incdir-install
	$E "  INSTALL $<"
	$Q$(INSTALL) -m644 $< $(DESTDIR)$(INCDIR)/$<

%.h-install: %.h incdir-install
	$E "  INSTALL $(notdir $<)"
	$Q$(INSTALL) -m644 $< $(DESTDIR)$(INCDIR)/lem/$(notdir $<)

path-install:
	$E "  INSTALL -d $(LUA_PATH)"
	$Q$(INSTALL) -d $(DESTDIR)$(LUA_PATH)/lem

%.lua-install: %.lua path-install
	$E "  INSTALL $<"
	$Q$(INSTALL) -m644 $< $(DESTDIR)$(LUA_PATH)/lem/$<

cpath-install:
	$E "  INSTALL -d $(LUA_CPATH)"
	$Q$(INSTALL) -d $(DESTDIR)$(LUA_CPATH)/lem

%.so-install: %.so cpath-install
	$E "  INSTALL $<"
	$Q$(INSTALL) $< $(DESTDIR)$(LUA_CPATH)/lem/$<

pkgdir-install:
	$E "  INSTALL -d $(PKG_CONFIG_PATH)"
	$Q$(INSTALL) -d $(DESTDIR)$(PKG_CONFIG_PATH)

%.pc-install: %.pc pkgdir-install
	$E "  INSTALL $<"
	$Q$(INSTALL) -m644 $< $(DESTDIR)$(PKG_CONFIG_PATH)/$<

install: lem.pc-install $(headers:%=%-install) $(programs:%=%-install) $(scripts:%=%-install)

clean:
	rm -f config.h lem.pc $(programs) *.o lua/*.o *.c~ *.h~
