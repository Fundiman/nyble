# NybleR build driver.
#
# Renderer dependencies are built locally from source submodules. No system
# Mesa, Tint, or Lexbor is consumed.

SHELL := /bin/sh

ifeq ($(OS),Windows_NT)
TINT_GCLIENT = cmd.exe /c "set DEPOT_TOOLS_UPDATE=0&& set PATH=$(DEPOT_TOOLS);%PATH% && gclient.bat sync --nohooks"
else
TINT_GCLIENT = DEPOT_TOOLS_UPDATE=0 PATH="$(DEPOT_TOOLS):$$PATH" gclient sync --nohooks
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

MESA_ARGS ?= -Dllvm=enabled -Dgallium-drivers=swrast -Dvulkan-drivers=swrast -Dosmesa=enabled -Degl=enabled -Dgles1=disabled -Dgles2=enabled

.PHONY: all deps lexbor tint mesa nybler check clean

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
	$(CMAKE) --build $(LEXBOR_BUILD) --config $(BUILD_TYPE) --parallel
	$(CMAKE) --install $(LEXBOR_BUILD) --config $(BUILD_TYPE)
	$(CMAKE) -E touch $@

$(DEPOT_TOOLS):
	$(CMAKE) -E make_directory $(CURDIR)/$(BUILD)
	git clone --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git $(DEPOT_TOOLS)

$(TINT_STAMP): $(DEPOT_TOOLS)
	$(CMAKE) -E make_directory $(STAMP_DIR)
	$(CMAKE) -E copy $(TINT_SRC)/standalone.gclient $(TINT_SRC)/.gclient
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
		-DTINT_BUILD_HLSL_WRITER=ON \
		-DTINT_BUILD_MSL_WRITER=ON \
		-DTINT_ENABLE_INSTALL=ON
	$(CMAKE) --build $(TINT_BUILD) --target tint_cmd_tint_cmd --parallel
	$(CMAKE) --install $(TINT_BUILD)
	$(CMAKE) -E touch $@

$(MESA_STAMP):
	$(CMAKE) -E make_directory $(STAMP_DIR)
	$(MESON) setup $(MESA_BUILD) $(MESA_SRC) \
		--prefix=$(PREFIX) \
		--buildtype=release \
		$(MESA_ARGS)
	$(MESON) compile -C $(MESA_BUILD)
	$(MESON) install -C $(MESA_BUILD)
	$(CMAKE) -E touch $@

$(NYBLER_STAMP):
	$(CMAKE) -E make_directory $(STAMP_DIR)
	$(CMAKE) -S $(CURDIR) -B $(NYBLER_BUILD) -G $(CMAKE_GENERATOR) \
		-DCMAKE_C_COMPILER=$(NYBLER_CC) \
		-DCMAKE_CXX_COMPILER=$(NYBLER_CXX) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_PREFIX_PATH=$(PREFIX) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX)
	$(CMAKE) --build $(NYBLER_BUILD) --config $(BUILD_TYPE) --parallel
	$(CMAKE) --install $(NYBLER_BUILD) --config $(BUILD_TYPE)
	$(CMAKE) -E touch $@

check: nybler
	@echo "NybleR C API version: $$($(CMAKE) -P $(CURDIR)/cmake/print-version.cmake 2>/dev/null || echo unversioned)"

clean:
	$(CMAKE) -E remove_directory $(CURDIR)/$(BUILD)
