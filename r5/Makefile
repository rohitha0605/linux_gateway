PROTO_DIR := ../proto
PROTO     := $(PROTO_DIR)/calc.proto
GEN_C     := calc.pb-c.c
GEN_H     := calc.pb-c.h

PKG ?= pkg-config
CFLAGS += -O2 -DTEST_STANDALONE $(shell $(PKG) --cflags protobuf-c 2>/dev/null)
LDLIBS += $(shell $(PKG) --libs protobuf-c 2>/dev/null)

# Homebrew fallback (macOS dev boxes)
ifeq ($(shell $(PKG) --exists protobuf-c || echo no),no)
CFLAGS += -I/opt/homebrew/include
LDLIBS += -L/opt/homebrew/lib -lprotobuf-c
endif

all: host-smoke

$(GEN_C) $(GEN_H): $(PROTO)
	@which protoc >/dev/null || { echo "need protoc"; exit 1; }
	@which protoc-gen-c >/dev/null || { echo "need protoc-gen-c"; exit 1; }
	protoc -I $(PROTO_DIR) --c_out=. --plugin=protoc-gen-c=$(shell which protoc-gen-c) $(PROTO)

echo.o: echo.c $(GEN_H)

host-smoke: echo.o $(GEN_C)
	$(CC) $(CFLAGS) -o $@ echo.o $(GEN_C) $(LDLIBS)

clean:
	rm -f *.o host-smoke $(GEN_C) $(GEN_H)
