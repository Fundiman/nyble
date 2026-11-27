# NybleR build driver.
#
# Renderer dependencies are built locally from source submodules. No system
# Mesa, Tint, or Lexbor is consumed.

SHELL := /bin/sh

ifeq ($(OS),Windows_NT)
PYTHON ?= python
TINT_GCLIENT = cmd.exe /c "set DEPOT_TOOLS_UPDATE=0&& set PATH=$(DEPOT_TOOLS);%PATH%&& $(PYTHON) $(DEPOT_TOOLS)/gclient.py sync --nohooks --ignore-dep-type=cipd"
else
PYTHON ?= python3
TINT_GCLIENT = DEPOT_TOOLS_UPDATE=0 PATH="$(DEPOT_TOOLS):$$PATH" $(PYTHON) $(DEPOT_TOOLS)/gclient.py sync --nohooks --ignore-dep-type=cipd
endif

BUILD ?= build
BUILD_TYPE ?= Release
PREFIX ?= $(CURDIR)/$(BUILD)/local

CMAKE ?= cmake
MESON ?= meson
NINJA ?= ninja
CMAKE_GENERATOR ?= Ninja
NYBLER_CC ?= clang
NYBLER_CXX ?= clang++
NYBLER_JOBS ?= 2
NYBLER_TMP := $(CURDIR)/$(BUILD)/tmp

ifeq ($(OS),Windows_NT)
NYBLER_TARGET ?= x86_64-w64-mingw32
NYBLER_MINGW_BIN ?= C:/ProgramData/mingw64/mingw64/bin
MESON_ENV = CC="$(NYBLER_CC)" CXX="$(NYBLER_CXX)" \
	CFLAGS="--target=$(NYBLER_TARGET)" CXXFLAGS="--target=$(NYBLER_TARGET)" \
	PATH="$(NYBLER_MINGW_BIN):$$PATH"
else
MESON_ENV =
endif

ROOT := $(abspath ..)
LEXBOR_SRC := $(ROOT)/third_party/lexbor
TINT_SRC := $(ROOT)/third_party/tint
MESA_SRC := $(ROOT)/third_party/mesa

LEXBOR_BUILD := $(CURDIR)/$(BUILD)/lexbor
TINT_BUILD := $(CURDIR)/$(BUILD)/tint
MESA_BUILD := $(CURDIR)/$(BUILD)/mesa
NYBLER_BUILD := $(CURDIR)/$(BUILD)/nybler
DEPOT_TOOLS ?= $(CURDIR)/$(BUILD)/depot_tools
STAMP_DIR := $(CURDIR)/$(BUILD)/stamps

LEXBOR_STAMP := $(STAMP_DIR)/lexbor.stamp
TINT_STAMP := $(STAMP_DIR)/tint.stamp
MESA_STAMP := $(STAMP_DIR)/mesa.stamp
NYBLER_STAMP := $(STAMP_DIR)/nybler.stamp

MESA_ARGS ?= -Dllvm=enabled -Dgallium-drivers=llvmpipe -Dvulkan-drivers=swrast -Degl=enabled -Dgles1=disabled -Dgles2=enabled

DOM_SMOKE_BIN := $(CURDIR)/$(BUILD)/dom-smoke$(if $(filter Windows_NT,$(OS)),.exe,)
ifeq ($(OS),Windows_NT)
LEXBOR_LIB ?= $(PREFIX)/lib/lexbor_static.lib
else
LEXBOR_LIB ?= $(PREFIX)/lib/liblexbor_static.a
endif

.PHONY: all deps lexbor tint mesa nybler dom-smoke check clean

all: deps nybler

deps: lexbor tint mesa

lexbor: $(LEXBOR_STAMP)
tint: $(TINT_STAMP)
mesa: $(MESA_STAMP)
nybler: $(NYBLER_STAMP)

$(LEXBOR_STAMP):
	$(CMAKE) -E make_directory $(STAMP_DIR)
	$(CMAKE) -S $(LEXBOR_SRC) -B $(LEXBOR_BUILD) -G $(CMAKE_GENERATOR) \
		-DCMAKE_C_COMPILER=$(NYBLER_CC) \
		-DCMAKE_CXX_COMPILER=$(NYBLER_CXX) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX) \
		-DLEXBOR_BUILD_SHARED=OFF \
		-DLEXBOR_BUILD_STATIC=ON \
		-DLEXBOR_BUILD_TESTS=OFF \
		-DLEXBOR_BUILD_EXAMPLES=OFF
	$(CMAKE) --build $(LEXBOR_BUILD) --config $(BUILD_TYPE) --parallel $(NYBLER_JOBS)
	$(CMAKE) --install $(LEXBOR_BUILD) --config $(BUILD_TYPE)
	$(CMAKE) -E touch $@

$(DEPOT_TOOLS):
	$(CMAKE) -E make_directory $(CURDIR)/$(BUILD)
	git clone --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git $(DEPOT_TOOLS)

$(TINT_STAMP): $(DEPOT_TOOLS)
	$(CMAKE) -E make_directory $(STAMP_DIR)
	$(CMAKE) -E make_directory $(NYBLER_TMP)
	$(CMAKE) -E copy $(CURDIR)/config/tint.gclient $(TINT_SRC)/.gclient
	$(CMAKE) -E rm -f $(TINT_SRC)/.gclient_entries
	cd $(TINT_SRC) && $(TINT_GCLIENT)
	$(CMAKE) -S $(TINT_SRC) -B $(TINT_BUILD) -G $(CMAKE_GENERATOR) \
		-DCMAKE_C_COMPILER=$(NYBLER_CC) \
		-DCMAKE_CXX_COMPILER=$(NYBLER_CXX) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX) \
		-DTINT_BUILD_TESTS=OFF \
		-DTINT_BUILD_CMD_TOOLS=ON \
		-DTINT_BUILD_FUZZERS=OFF \
		-DTINT_BUILD_BENCHMARKS=OFF \
		-DTINT_BUILD_REMOTE_COMPILE=OFF \
		-DTINT_BUILD_SPV_READER=ON \
		-DTINT_BUILD_SPV_WRITER=ON \
		-DTINT_BUILD_WGSL_READER=ON \
		-DTINT_BUILD_WGSL_WRITER=ON \
		-DTINT_BUILD_GLSL_WRITER=ON \
		-DTINT_BUILD_HLSL_WRITER=OFF \
		-DTINT_BUILD_MSL_WRITER=ON \
		-DTINT_ENABLE_INSTALL=ON
	TMPDIR=$(NYBLER_TMP) TMP=$(NYBLER_TMP) TEMP=$(NYBLER_TMP) $(CMAKE) --build $(TINT_BUILD) --parallel $(NYBLER_JOBS)
	TMPDIR=$(NYBLER_TMP) TMP=$(NYBLER_TMP) TEMP=$(NYBLER_TMP) $(CMAKE) --build $(TINT_BUILD) --target tint_utils_bytes --parallel $(NYBLER_JOBS)
	TMPDIR=$(NYBLER_TMP) TMP=$(NYBLER_TMP) TEMP=$(NYBLER_TMP) $(CMAKE) --install $(TINT_BUILD)
	$(CMAKE) -E touch $@

$(MESA_STAMP):
	$(CMAKE) -E make_directory $(STAMP_DIR)
	$(CMAKE) -E make_directory $(NYBLER_TMP)
	$(MESON_ENV) TMPDIR=$(NYBLER_TMP) TMP=$(NYBLER_TMP) TEMP=$(NYBLER_TMP) $(MESON) setup $(MESA_BUILD) $(MESA_SRC) \
		--prefix=$(PREFIX) \
		--buildtype=release \
		$(MESA_ARGS)
	$(MESON_ENV) TMPDIR=$(NYBLER_TMP) TMP=$(NYBLER_TMP) TEMP=$(NYBLER_TMP) $(MESON) compile -C $(MESA_BUILD) -j $(NYBLER_JOBS)
	$(MESON_ENV) TMPDIR=$(NYBLER_TMP) TMP=$(NYBLER_TMP) TEMP=$(NYBLER_TMP) $(MESON) install -C $(MESA_BUILD)
	$(CMAKE) -E touch $@

$(NYBLER_STAMP):
	$(CMAKE) -E make_directory $(STAMP_DIR)
	$(CMAKE) -S $(CURDIR) -B $(NYBLER_BUILD) -G $(CMAKE_GENERATOR) \
		-DCMAKE_C_COMPILER=$(NYBLER_CC) \
		-DCMAKE_CXX_COMPILER=$(NYBLER_CXX) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_PREFIX_PATH=$(PREFIX) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX)
	$(CMAKE) --build $(NYBLER_BUILD) --config $(BUILD_TYPE) --parallel $(NYBLER_JOBS)
	$(CMAKE) --install $(NYBLER_BUILD) --config $(BUILD_TYPE)
	$(CMAKE) -E touch $@

dom-smoke: lexbor
	$(CMAKE) -E make_directory $(CURDIR)/$(BUILD)
	$(NYBLER_CC) -std=c11 -DLEXBOR_STATIC -I$(PREFIX)/include \
		-o $(DOM_SMOKE_BIN) $(CURDIR)/tests/dom_smoke.c $(LEXBOR_LIB)
	$(DOM_SMOKE_BIN)

check: nybler dom-smoke
	@echo "NybleR C API version: $$($(CMAKE) -P $(CURDIR)/cmake/print-version.cmake 2>/dev/null || echo unversioned)"

clean:
	$(CMAKE) -E remove_directory $(CURDIR)/$(BUILD)
