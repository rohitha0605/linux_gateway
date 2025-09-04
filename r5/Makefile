CC ?= gcc
CFLAGS ?= -O2

# Use pkg-config for protobuf-c flags
CFLAGS  += $(shell pkg-config --cflags protobuf-c)
LDLIBS  += $(shell pkg-config --libs protobuf-c)

all: host-smoke

# Generate C stubs from ../proto/calc.proto
calc.pb-c.c calc.pb-c.h: ../proto/calc.proto
	protoc -I ../proto --plugin=protoc-gen-c=$(shell which protoc-gen-c) --c_out=. $<

host-smoke: echo.o calc.pb-c.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f host-smoke *.o calc.pb-c.c calc.pb-c.h gen-req
