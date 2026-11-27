# NybleJS + Ant Silver (libant) build.
#
# Host detection uses `uname`. POSIX hosts build a native libant embed with
# `make ant-posix`; Windows hosts keep the mingw-w64/Zig path and consume the
# libant-mingw64 CI artifact (or a local embed dist) via ANT_DIR.

NYBLE_UNAME_S := $(shell uname -s 2>/dev/null)
NYBLE_HOST_ID := $(if $(NYBLE_UNAME_S),$(NYBLE_UNAME_S),$(OS))
ifneq (,$(filter Windows_NT MINGW% MSYS% CYGWIN%,$(NYBLE_HOST_ID)))
  HOST_WINDOWS := 1
else
  HOST_POSIX := 1
endif

MESON ?= meson
PKG_CONFIG ?= pkg-config
LTO ?= 0
ifeq ($(LTO),1)
LTO_FLAG = -flto
else
LTO_FLAG =
endif

ANT_SOURCE ?= ../third_party/ant
ANT_POSIX_BUILD ?= build-embed-posix

ifdef HOST_POSIX
ifeq ($(origin CC),default)
CC = clang
endif
ifeq ($(origin CXX),default)
CXX = clang++
endif
ANT_DIR ?= $(ANT_SOURCE)/$(ANT_POSIX_BUILD)/libant
else
# Windows: keep the existing mingw-w64 ABI path. USE_ZIG=1 selects
# `zig cc`/`zig c++` with x86_64-windows-gnu instead of MSYS2 clang.
USE_ZIG ?= 0
ifeq ($(USE_ZIG),1)
CC = zig cc
CXX = zig c++
TARGET_TRIPLE ?= x86_64-windows-gnu
else
ifeq ($(origin CC),default)
CC = clang
endif
ifeq ($(origin CXX),default)
CXX = clang++
endif
TARGET_TRIPLE ?= x86_64-w64-mingw32
endif
ANT_DIR ?= ../tools/ant
endif

ANT_LIB := $(ANT_DIR)/lib/libant.a
ANT_INC := $(ANT_DIR)/include

WARN = -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
INCLUDES = -I$(ANT_INC) -I$(ANT_INC)/ant -Isrc
EXAMPLE_INCLUDES = $(INCLUDES) -I.

ifdef HOST_POSIX
# The embed dist pkg-config file records -pthread, -no-pie (Linux) and the
# Darwin frameworks/libs needed by the static archive.
ANT_PKG_CFLAGS = $(shell PKG_CONFIG_PATH=$(ANT_DIR)/lib/pkgconfig $(PKG_CONFIG) --cflags libant 2>/dev/null)
ANT_SYS_LIBS = $(shell PKG_CONFIG_PATH=$(ANT_DIR)/lib/pkgconfig $(PKG_CONFIG) --libs libant 2>/dev/null || printf '%s' '-pthread -lm -ldl')
CXXFLAGS = -std=c++17 -O2 $(LTO_FLAG) $(WARN) $(INCLUDES) $(ANT_PKG_CFLAGS)
CFLAGS = -std=gnu2x -O2 $(LTO_FLAG) $(WARN) $(INCLUDES) $(ANT_PKG_CFLAGS) -DNYBLE_ANT_PACKAGED
EXAMPLE_CXXFLAGS = -std=c++17 -O2 $(LTO_FLAG) $(WARN) $(EXAMPLE_INCLUDES) $(ANT_PKG_CFLAGS)
LDFLAGS = -O2 $(LTO_FLAG)
NJS = njs
EMBED_SM = build/embed_sm
EMBED_ENGINE = build/embed_engine
else
TARGET_FLAG = --target=$(TARGET_TRIPLE)
CXXFLAGS = $(TARGET_FLAG) -std=c++17 -O2 $(LTO_FLAG) $(WARN) $(INCLUDES)
CFLAGS = $(TARGET_FLAG) -std=gnu2x -O2 $(LTO_FLAG) $(WARN) $(INCLUDES) -DNYBLE_ANT_PACKAGED
EXAMPLE_CXXFLAGS = $(TARGET_FLAG) -std=c++17 -O2 $(LTO_FLAG) $(WARN) $(EXAMPLE_INCLUDES)
# pathcch_shim.o supplies PathCchCombine; sync_shim.o supplies WaitOnAddress.
LDFLAGS = $(TARGET_FLAG) -O2 $(LTO_FLAG) -fuse-ld=lld -s -static-libstdc++ -static-libgcc
ANT_SYS_LIBS = -lws2_32 -lcrypt32 -lsecur32 -luserenv -lntdll -liphlpapi -ldbghelp -lole32 -loleaut32 -luuid -lshell32 -ladvapi32 -lbcrypt -lwinmm -lnormaliz "-Wl,-Bstatic" -lwinpthread "-Wl,-Bdynamic"
WIN_SHIMS = build/pathcch_shim.o build/sync_shim.o
NJS = njs.exe
EMBED_SM = build/embed_sm.exe
EMBED_ENGINE = build/embed_engine.exe
endif

ENGINE_OBJS = build/engine/ant_engine.o build/engine/ant_engine.c.o build/engine/nyble_sm.c.o $(WIN_SHIMS)
CLI_OBJS = build/main.o $(ENGINE_OBJS)
EMBED_SM_OBJ = build/examples/embed_sm.o
EMBED_ENGINE_OBJ = build/examples/embed_engine.o

all: $(NJS)

embed: $(EMBED_SM) $(EMBED_ENGINE)

$(NJS): $(ANT_LIB) $(CLI_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(CLI_OBJS) $(ANT_LIB) $(ANT_SYS_LIBS)

$(EMBED_SM): $(ANT_LIB) $(ENGINE_OBJS) $(EMBED_SM_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(EMBED_SM_OBJ) $(ENGINE_OBJS) $(ANT_LIB) $(ANT_SYS_LIBS)

$(EMBED_ENGINE): $(ANT_LIB) $(ENGINE_OBJS) $(EMBED_ENGINE_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(EMBED_ENGINE_OBJ) $(ENGINE_OBJS) $(ANT_LIB) $(ANT_SYS_LIBS)

build/main.o: src/main.cpp | build/engine
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/engine/%.o: src/engine/%.cpp | build/engine
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/engine/%.c.o: src/engine/%.c | build/engine
	$(CC) $(CFLAGS) -c $< -o $@

build/examples/%.o: examples/%.cpp | build/examples
	$(CXX) $(EXAMPLE_CXXFLAGS) -c $< -o $@

ifdef HOST_WINDOWS
build/pathcch_shim.o: build/pathcch_shim.c | build/engine
	$(CC) $(TARGET_FLAG) -std=gnu2x -O2 -c $< -o $@

build/sync_shim.o: build/sync_shim.c | build/engine
	$(CC) $(TARGET_FLAG) -std=gnu2x -O2 -c $< -o $@
endif

build/engine build/examples:
	@mkdir -p $@

# POSIX Ant embed: no MSYS2, no WSL, no BoringSSL, no Rust bootstrap.
ANT_POSIX_FLAGS = -Db_lto=false -Dc_std=gnu2x -Dtls_backend=mbedtls -Dtemporal=disabled -Dencoding_rs=disabled -Dsandbox=disabled -Dpgo=disabled -Dcodesign=false
ANT_POSIX_CONFIGURED := $(wildcard $(ANT_SOURCE)/$(ANT_POSIX_BUILD)/build.ninja)
ifeq ($(ANT_POSIX_CONFIGURED),)
ANT_POSIX_SETUP = $(MESON) setup $(ANT_POSIX_BUILD) $(ANT_POSIX_FLAGS)
else
ANT_POSIX_SETUP = $(MESON) setup --reconfigure $(ANT_POSIX_BUILD) $(ANT_POSIX_FLAGS)
endif

ifdef HOST_POSIX
ant-posix:
	cd $(ANT_SOURCE) && $(ANT_POSIX_SETUP)
	cd $(ANT_SOURCE) && $(MESON) compile -C $(ANT_POSIX_BUILD) embed
else
ant-posix:
	@echo "ant-posix requires a POSIX host; on Windows use the mingw-w64 embed artifact"; exit 1
endif

$(ANT_LIB):
ifdef HOST_POSIX
	@echo "Missing $(ANT_LIB). Run 'make ant-posix' or set ANT_DIR to a POSIX libant embed dist."; exit 1
else
	@echo "Missing $(ANT_LIB). Download the libant-mingw64 CI artifact into $(ANT_DIR)/ or set ANT_DIR to a local embed dist."; exit 1
endif

clean:
	rm -f $(CLI_OBJS) $(EMBED_SM_OBJ) $(EMBED_ENGINE_OBJ) $(EMBED_SM) $(EMBED_ENGINE) njs njs.exe
	rm -rf build/engine build/examples

run: $(NJS)
	./$(NJS)

.PHONY: all embed clean run ant-posix
