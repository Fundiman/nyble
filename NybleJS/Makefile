# NybleJS + Ant Silver (libant) build
# Requires a prebuilt libant embed dist (CI artifact: libant-mingw64).
#   ANT_DIR=../tools/ant  (default) should contain lib/libant.a and include/

CXX ?= clang++
CC  ?= clang
TARGET_TRIPLE ?= x86_64-w64-mingw32

ANT_DIR ?= ../tools/ant
ANT_LIB := $(ANT_DIR)/lib/libant.a
ANT_INC := $(ANT_DIR)/include

LTO ?= 0
ifeq ($(LTO),1)
LTO_FLAG = -flto
else
LTO_FLAG =
endif

WARN = -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
CXXFLAGS = --target=$(TARGET_TRIPLE) -std=c++17 -O2 $(LTO_FLAG) $(WARN) -I$(ANT_INC) -I$(ANT_INC)/ant -Isrc
CFLAGS   = --target=$(TARGET_TRIPLE) -std=gnu2x -O2 $(LTO_FLAG) $(WARN) -I$(ANT_INC) -I$(ANT_INC)/ant -Isrc -DNYBLE_ANT_PACKAGED

# Windows/mingw-w64 system libs needed by static libant.a (Ant + tlsuv + mbedTLS).
ANT_SYS_LIBS = -lws2_32 -lcrypt32 -lsecur32 -luserenv -lntdll -liphlpapi -ldbghelp -lole32 -luuid -lshell32 -ladvapi32 -lbcrypt -lwinmm -lpathcch -lnormaliz
LDFLAGS = --target=$(TARGET_TRIPLE) -O2 $(LTO_FLAG) -s -static-libstdc++

SRCS_CPP = src/main.cpp src/engine/ant_engine.cpp
SRCS_C   = src/engine/ant_engine.c
OBJS_CPP = $(SRCS_CPP:src/%.cpp=build/%.o)
OBJS_C   = $(SRCS_C:src/%.c=build/%.o)
OBJS     = $(OBJS_CPP) $(OBJS_C)

all: njs

njs: $(ANT_LIB) $(OBJS)
	$(CXX) $(LDFLAGS) -o njs $(OBJS) $(ANT_LIB) $(ANT_SYS_LIBS)

build/%.o: src/%.cpp | build/engine
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/%.o: src/%.c | build/engine
	$(CC) $(CFLAGS) -c $< -o $@

build/engine:
	@mkdir -p build/engine

$(ANT_LIB):
	@echo "Missing $(ANT_LIB). Download the libant-mingw64 CI artifact into $(ANT_DIR)/"; exit 1

clean:
	rm -rf build njs njs.exe

run: njs
	./njs

.PHONY: all clean run