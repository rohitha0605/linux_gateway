PKG := protobuf-c
CFLAGS := $(shell pkg-config --cflags $(PKG))
LIBS := $(shell pkg-config --libs $(PKG))

all: host-smoke

calc.pb-c.c calc.pb-c.h: ../proto/calc.proto
	protoc-c --c_out=. $<

host-smoke: calc.pb-c.c host_roundtrip.c echo_calc.c
	$(CC) -O2 -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm -f host_smoke *.o calc.pb-c.c calc.pb-c.h

.PHONY: all clean
