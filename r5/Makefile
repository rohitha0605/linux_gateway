BREW_PREFIX ?= $(shell brew --prefix 2>/dev/null)

PKG_CFLAGS := $(shell sh -c '\
  pkg-config --cflags protobuf-c      2>/dev/null || \
  pkg-config --cflags libprotobuf-c   2>/dev/null || \
  pkg-config --cflags protobuf-c-1.0  2>/dev/null || \
  echo -I$(BREW_PREFIX)/opt/protobuf-c/include')

PKG_LIBS := $(shell sh -c '\
  pkg-config --libs protobuf-c        2>/dev/null || \
  pkg-config --libs libprotobuf-c     2>/dev/null || \
  pkg-config --libs protobuf-c-1.0    2>/dev/null || \
  echo -L$(BREW_PREFIX)/opt/protobuf-c/lib -lprotobuf-c')

CFLAGS ?= -O2 -DTEST_STANDALONE
CFLAGS += $(PKG_CFLAGS)

all: host-smoke

calc.pb-c.c calc.pb-c.h: ../proto/calc.proto
	@protoc -I ../proto --plugin=protoc-gen-c=$$(command -v protoc-gen-c) --c_out=. $<

echo.o: echo.c calc.pb-c.h
	$(CC) $(CFLAGS) -c $< -o $@

calc.pb-c.o: calc.pb-c.c calc.pb-c.h
	$(CC) $(CFLAGS) -c $< -o $@

host-smoke: echo.o calc.pb-c.o
	$(CC) $(CFLAGS) $^ $(PKG_LIBS) -o $@

clean:
	rm -f host-smoke *.o calc.pb-c.c calc.pb-c.h
