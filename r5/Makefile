PROTO_DIR := ../proto
PROTO := $(PROTO_DIR)/calc.proto
GEN_C := calc.pb-c.c
GEN_H := calc.pb-c.h

PKG_CFLAGS := $(shell pkg-config --cflags protobuf-c 2>/dev/null)
PKG_LIBS   := $(shell pkg-config --libs protobuf-c 2>/dev/null)
ifeq ($(PKG_CFLAGS),)
  PKG_CFLAGS := -I/opt/homebrew/include
endif
ifeq ($(PKG_LIBS),)
  PKG_LIBS := -L/opt/homebrew/lib -lprotobuf-c
endif

all: host-smoke

$(GEN_C) $(GEN_H): $(PROTO)
	@set -e; \
	if command -v protoc-c >/dev/null 2>&1; then \
	  protoc-c -I $(PROTO_DIR) --c_out=. $(PROTO); \
	elif command -v protoc >/devnull 2>&1 && command -v protoc-gen-c >/dev/null 2>&1; then \
	  protoc -I $(PROTO_DIR) --plugin=protoc-gen-c=$$(command -v protoc-gen-c) --c_out=. $(PROTO); \
	else \
	  echo "Need protoc-c or protoc+protoc-gen-c"; exit 1; \
	fi

CFLAGS := -O2 $(PKG_CFLAGS)
LDFLAGS := $(PKG_LIBS)

host-smoke: echo.o $(GEN_C)
	$(CC) -o $@ echo.o $(GEN_C) $(LDFLAGS)

echo.o: echo.c $(GEN_H)
	$(CC) $(CFLAGS) -DTEST_STANDALONE -c echo.c -o echo.o

clean:
	rm -f host-smoke echo.o $(GEN_C) $(GEN_H)
