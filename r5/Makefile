PKG_CONFIG ?= pkg-config

# Choose the right .pc: Homebrew=protobuf-c, Debian/Ubuntu=libprotobuf-c
PKG_NAME   := $(shell $(PKG_CONFIG) --exists protobuf-c    && echo protobuf-c || \
                       $(PKG_CONFIG) --exists libprotobuf-c && echo libprotobuf-c)

PKG_CFLAGS := $(shell [ -n "$(PKG_NAME)" ] && $(PKG_CONFIG) --cflags $(PKG_NAME))
PKG_LIBS   := $(shell [ -n "$(PKG_NAME)" ] && $(PKG_CONFIG) --libs   $(PKG_NAME))

# macOS fallback (dev machines) if pkg-config didn’t find anything
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  PKG_CFLAGS ?= -I$(shell brew --prefix protobuf-c)/include
  PKG_LIBS   ?= -L$(shell brew --prefix protobuf-c)/lib -lprotobuf-c
endif

CFLAGS  += -O2 -DTEST_STANDALONE $(PKG_CFLAGS)
LDFLAGS += $(PKG_LIBS)

all: host-smoke

calc.pb-c.c calc.pb-c.h: ../proto/calc.proto
	@protoc -I ../proto --plugin=protoc-gen-c=$$(command -v protoc-gen-c) --c_out=. $<

echo.o: echo.c calc.pb-c.h
	$(CC) $(CFLAGS) -c echo.c -o $@

calc.pb-c.o: calc.pb-c.c calc.pb-c.h
	$(CC) $(CFLAGS) -c calc.pb-c.c -o $@

host-smoke: echo.o calc.pb-c.o
	$(CC) $(CFLAGS) echo.o calc.pb-c.o $(LDFLAGS) -o $@

clean:
	rm -f host-smoke *.o calc.pb-c.c calc.pb-c.h

.PHONY: all clean
