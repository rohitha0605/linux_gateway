.PHONY: all clean
CC      ?= cc
CFLAGS  ?= -O2
PROTO    = ../proto/calc.proto

# Homebrew prefix (mac) or /opt/homebrew as default
BREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo /opt/homebrew)

# Prefer pkg-config; fall back to common Homebrew include/lib dirs
PKG_CFLAGS := $(shell pkg-config --cflags protobuf-c 2>/dev/null)
PKG_LIBS   := $(shell pkg-config --libs   protobuf-c 2>/dev/null)
CFLAGS     += $(if $(PKG_CFLAGS),$(PKG_CFLAGS),-I$(BREW_PREFIX)/include)
LDLIBS     += $(if $(PKG_LIBS),$(PKG_LIBS),-L$(BREW_PREFIX)/lib -lprotobuf-c)

PROTOC       ?= protoc
PROTOC_GEN_C ?= $(shell command -v protoc-gen-c || echo protoc-gen-c)

all: host-smoke

calc.pb-c.c calc.pb-c.h: $(PROTO)
	$(PROTOC) -I ../proto --plugin=protoc-gen-c=$(PROTOC_GEN_C) --c_out=. $(PROTO)

echo.o: echo.c calc.pb-c.h
calc.pb-c.o: calc.pb-c.c calc.pb-c.h

host-smoke: echo.o calc.pb-c.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f host-smoke *.o calc.pb-c.c calc.pb-c.h
