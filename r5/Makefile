BREW_PREFIX ?= $(shell brew --prefix protobuf-c 2>/dev/null || echo /opt/homebrew/opt/protobuf-c)

# Prefer pkg-config if available; otherwise fall back to Homebrew paths
ifneq ($(shell command -v pkg-config >/dev/null 2>&1 && echo yes),)
  ifeq ($(shell pkg-config --exists protobuf-c && echo yes),yes)
    PKG_CFLAGS := $(shell pkg-config --cflags protobuf-c)
    PKG_LIBS   := $(shell pkg-config --libs   protobuf-c)
  else
    PKG_CFLAGS := -I$(BREW_PREFIX)/include
    PKG_LIBS   := -L$(BREW_PREFIX)/lib -lprotobuf-c
  endif
else
  PKG_CFLAGS := -I$(BREW_PREFIX)/include
  PKG_LIBS   := -L$(BREW_PREFIX)/lib -lprotobuf-c
endif

CFLAGS ?= -O2 -DTEST_STANDALONE

all: host-smoke

host-smoke: echo.o calc.pb-c.o
	$(CC) $(CFLAGS) $^ $(PKG_LIBS) -o $@

echo.o: echo.c calc.pb-c.h
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -c $< -o $@

calc.pb-c.o: calc.pb-c.c calc.pb-c.h
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -c $< -o $@

calc.pb-c.c calc.pb-c.h: ../proto/calc.proto
	protoc -I ../proto --plugin=protoc-gen-c=$(shell command -v protoc-gen-c) --c_out=. $<

clean:
	rm -f host-smoke *.o calc.pb-c.c calc.pb-c.h

.PHONY: all clean
