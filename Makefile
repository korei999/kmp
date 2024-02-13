MAKEFLAGS := --jobs=$(shell nproc) --output-sync=target 

include dbg.mk

CC := g++ -fdiagnostics-color=always
WARNING := -Wall -Wextra -Wpedantic

OPUS := $(shell pkg-config --cflags opusfile opus)
OPUS_LIB := $(shell pkg-config --libs opus opusfile)

NCURSES := $(shell pkg-config --cflags ncursesw)
NCURSES_LIB := $(shell pkg-config --libs ncursesw)

CFLAGS := -std=c++2b -pipe $(OPUS) $(NCURSES)
LDFLAGS := -lasound -lm $(OPUS_LIB) $(NCURSES_LIB)

SRCD := .
BD := ./build
BIN := kmp
EXEC := $(BD)/$(BIN)

SRCS := $(shell find $(SRCD) -name '*.cc')
OBJ := $(SRCS:%=$(BD)/%.o)

# release build
all: CC += -flto=auto $(SAFE_STACK) 
all: CFLAGS += -O2 -march=sandybridge $(WARNING) -DNDEBUG
all: $(EXEC)

# debug build
debug: CC += #$(ASAN)
debug: CFLAGS += -O0 $(DEBUG) $(WARNING) $(WNO)
debug: LDFLAGS += -fuse-ld=mold
debug: $(EXEC)

# arcane rules to build everything
$(EXEC): $(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BD)/%.cc.o: %.cc Makefile dbg.mk *.hh
	mkdir -p $(BD)
	$(CC) $(CFLAGS) -c $< -o $@

PREFIX = /usr/local

install: $(EXEC)
	strip $(EXEC)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(EXEC) $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(BIN)
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)

.PHONY: clean

clean:
	rm -rf $(BD)
