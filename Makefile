CC      = gcc
CFLAGS  ?= -O2 -pipe -Wall -Wextra -Wno-variadic-macros -Wno-strict-aliasing
STRIP   = strip
INSTALL = install
SED     = sed

LUA_VERSION = 5.1
DESTDIR     =
PREFIX      = /usr/local
BINDIR      = $(PREFIX)/bin
LIBDIR      = $(PREFIX)/lib/lua/$(LUA_VERSION)
INCDIR      = $(PREFIX)/include

OS = $(shell uname)

ifeq ($(OS), Linux)
DL = -ldl
endif

headers  = lem.h config.h macros.h lua/luaconf.h lua/lua.h lua/lauxlib.h libev/ev.h
programs = lem utils.so
scripts  = repl.lua lem-repl

ifdef NDEBUG
DEFINES += -DNDEBUG
endif

ifdef V
M=@\#
O=
else
M=@
O=@
endif

# From lua/Makefile
CORE_O=	lapi.o lcode.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o lmem.o \
	lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o  \
	lundump.o lvm.o lzio.o
LIB_O=	lauxlib.o lbaselib.o ldblib.o liolib.o lmathlib.o loslib.o ltablib.o \
	lstrlib.o loadlib.o linit.o

.PHONY: all lua strip install clean
.PRECIOUS: %.pic.o

all: $(programs)

config.h: config.$(OS)
	$Mecho '  SED $@'
	$O$(SED) -e 's|@PREFIX@|$(PREFIX)/|' $< > $@

event.o: event.c config.h
	$Mecho '  CC $@'
	$O$(CC) $(CFLAGS) -Iinclude -w $(DEFINES) -c $<

%.pic.o: %.c config.h
	$Mecho '  CC $@'
	$O$(CC) $(CFLAGS) -Iinclude -fPIC -nostartfiles $(DEFINES) -c $< -o $@

%.o: %.c %.h config.h
	$Mecho '  CC $@'
	$O$(CC) $(CFLAGS) -Iinclude $(DEFINES) -c $< -o $@

%.o: %.c config.h
	$Mecho '  CC $@'
	$O$(CC) $(CFLAGS) -Iinclude $(DEFINES) -c $< -o $@

lem: $(CORE_O:%=lua/%) $(LIB_O:%=lua/%) event.o lem.o
	$Mecho '  LD $@'
	$O$(CC) -rdynamic -lm $(DL) $(LDFLAGS) $^ -o $@

utils.so: utils.pic.o
	$Mecho '  LD $@'
	$O$(CC) -shared $(LDFLAGS) $^ -o $@

%-strip: %
	$Mecho '  STRIP $<'
	$O$(STRIP) $<

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
	$O$(INSTALL) $< $(DESTDIR)$(INCDIR)/$<

%.h-install: %.h incdir-install
	$Mecho "  INSTALL $(notdir $<)"
	$O$(INSTALL) $< $(DESTDIR)$(INCDIR)/lem/$(notdir $<)

libdir-install:
	$Mecho "  INSTALL -d $(LIBDIR)"
	$O$(INSTALL) -d $(DESTDIR)$(LIBDIR)/lem

%.so-install: %.so libdir-install
	$Mecho "  INSTALL $<"
	$O$(INSTALL) $< $(DESTDIR)$(LIBDIR)/lem/$<

%.lua-install: %.lua libdir-install
	$Mecho "  INSTALL $<"
	$O$(INSTALL) $< $(DESTDIR)$(LIBDIR)/lem/$<

install: $(headers:%=%-install) $(programs:%=%-install) $(scripts:%=%-install)

clean:
	rm -f config.h $(programs) *.o lua/*.o *.c~ *.h~
