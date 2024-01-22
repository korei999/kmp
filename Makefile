MAKEFLAGS := --jobs=$(shell nproc) --output-sync=target 

include dbg.mk

CC := g++ -fdiagnostics-color=always
WARNING := -Wall -Wextra -Wpedantic
DEBUG := -g

OPUS := $(shell pkg-config --cflags opusfile opus)
OPUS_LIB := $(shell pkg-config --libs opus opusfile)

CFLAGS := -std=c++20 -pipe $(OPUS)
LDFLAGS := -lasound -lm $(OPUS_LIB)

SRCD := .
BD := ./build
BIN := kmp

SRCS := $(shell find $(SRCD) -name '*.cc')
OBJ := $(SRCS:%=$(BD)/%.o)

# release build
all: CC += -flto=auto $(SAFE_STACK) 
all: CFLAGS += -O2 -march=sandybridge $(WARNING) -DNDEBUG
all: $(BD)/$(BIN)

# debug build
debug: CC += $(ASAN)
debug: CFLAGS += -O0 $(DEBUG) $(WARNING) $(WNO)
debug: $(BD)/$(BIN)

# arcane rules to build everything
$(BD)/$(BIN): $(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BD)/%.cc.o: %.cc Makefile dbg.mk *.hh
	mkdir -p $(BD)
	$(CC) $(CFLAGS) -c $< -o $@

PREFIX = /usr/local

install: $(BD)/$(BIN)
	strip $(BD)/$(BIN)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(BD)/$(BIN) $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(BIN)
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)

.PHONY: clean

clean:
	rm -rf $(BD)
