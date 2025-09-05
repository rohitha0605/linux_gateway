.PHONY: all clean

PKG_CFLAGS := $(shell pkg-config --cflags protobuf-c 2>/dev/null)
PKG_LIBS   := $(shell pkg-config --libs   protobuf-c 2>/dev/null)

CFLAGS    += -O2 -DTEST_STANDALONE $(PKG_CFLAGS)
LDFLAGS   += $(PKG_LIBS)

all: host-smoke

calc.pb-c.c calc.pb-c.h: ../proto/calc.proto
	@protoc -I ../proto --c_out=. --plugin=protoc-gen-c=$$(command -v protoc-gen-c) $<

echo.o: echo.c calc.pb-c.h
	$(CC) $(CFLAGS) -c echo.c -o $@

calc.pb-c.o: calc.pb-c.c calc.pb-c.h
	$(CC) $(CFLAGS) -c calc.pb-c.c -o $@

host-smoke: echo.o calc.pb-c.o
	$(CC) $(CFLAGS) echo.o calc.pb-c.o $(LDFLAGS) -o $@

clean:
	rm -f host-smoke *.o calc.pb-c.c calc.pb-c.h
