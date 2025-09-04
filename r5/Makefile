.PHONY: all clean
CC ?= cc
CFLAGS ?= -O2
PROTO = ../proto/calc.proto

CFLAGS  += $(shell pkg-config --cflags protobuf-c)
LDLIBS  += $(shell pkg-config --libs protobuf-c)

all: host-smoke

calc.pb-c.c calc.pb-c.h: $(PROTO)
	protoc -I ../proto --plugin=protoc-gen-c=$(shell which protoc-gen-c) --c_out=. $(PROTO)

echo.o: echo.c calc.pb-c.h
calc.pb-c.o: calc.pb-c.c calc.pb-c.h

host-smoke: echo.o calc.pb-c.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f host-smoke *.o calc.pb-c.c calc.pb-c.h
