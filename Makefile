MAKEFLAGS := --jobs=$(shell nproc) --output-sync=target 

include dbg.mk

CC := clang++ -stdlib=libc++ -fcolor-diagnostics -fansi-escape-codes -fdiagnostics-format=msvc
WARNING := -Wall -Wextra -Wpedantic

PGKS := alsa opusfile opus ncursesw
PKG := $(shell pkg-config --cflags $(PGKS))
PKG_LIB := $(shell pkg-config --libs $(PGKS))

CFLAGS := -std=c++23 -pipe $(PKG)
LDFLAGS := -lm $(PKG_LIB)

SRCD := .
BD := ./build
BIN := kmp
EXEC := $(BD)/$(BIN)

SRCS := $(shell find $(SRCD) -name '*.cc')
OBJ := $(SRCS:%=$(BD)/%.o)

# release build
all: CC += -flto=thin $(SAFE_STACK) 
all: CFLAGS += -g -O3 -march=sandybridge $(WARNING) -DNDEBUG
all: LDFLAGS += -fuse-ld=lld
all: $(EXEC)

# debug build
debug: CC += #$(ASAN)
debug: CFLAGS += -g -O0 $(DEBUG) $(WARNING) $(WNO)
debug: LDFLAGS += -fuse-ld=lld
debug: $(EXEC)

# build with gcc
gcc: CC := g++ -fdiagnostics-color=always
gcc: CFLAGS += -g -O3 -march=sandybridge $(WARNING) -DNDEBUG
gcc: $(EXEC)

# rules to build everything
$(EXEC): $(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BD)/%.cc.o: %.cc Makefile dbg.mk *.hh
	mkdir -p $(BD)
	$(CC) $(CFLAGS) -c $< -o $@

PREFIX = /usr/local

install: $(EXEC)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(EXEC) $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(BIN)
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)

.PHONY: clean tags

clean:
	rm -rf $(BD)

tags:
	ctags -R --language-force=C++ --extras=+q+r --c++-kinds=+p+l+x+L+A+N+U+Z
