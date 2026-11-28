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

NYBLER_MESON ?= $(PYTHON) $(CURDIR)/tools/meson_clock.py

# Keep this in a variable: commas inside $(if ...) would be parsed as argument
# separators. Meson looks for versioned python3.X shims on Windows.
ifeq ($(OS),Windows_NT)
PY_WRAPPERS = $(PYTHON) -c "import os,sys; d=r'$(PREFIX)/bin'; v='python%d.%d.bat' % (sys.version_info[0], sys.version_info[1]); data='@' + sys.executable + ' %*\n'; open(os.path.join(d, 'python3.bat'), 'w').write(data); open(os.path.join(d, v), 'w').write(data)"
else
PY_WRAPPERS =
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
# Mesa's generated api_trace.c makes clang 22 exhaust the Windows paging file
# even at -O0. MinGW GCC handles that file, so use it for the local Mesa build.
MESA_CC ?= $(if $(filter Windows_NT,$(OS)),gcc,$(NYBLER_CC))
MESA_CXX ?= $(if $(filter Windows_NT,$(OS)),g++,$(NYBLER_CXX))
NYBLER_TMP := $(CURDIR)/$(BUILD)/tmp
ifeq ($(OS),Windows_NT)
POWERSHELL ?= C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe
endif

ifeq ($(OS),Windows_NT)
NYBLER_TARGET ?= x86_64-w64-mingw32
NYBLER_MINGW_BIN ?= C:/ProgramData/mingw64/mingw64/bin
NYBLER_MINGW_LIB ?= C:/ProgramData/mingw64/mingw64/lib
NYBLER_TARGET_FLAGS ?= --target=$(NYBLER_TARGET) -L$(NYBLER_MINGW_LIB) -fuse-ld=bfd
NYBLER_PATH_ENV = PATH="$(NYBLER_MINGW_BIN):$(PREFIX)/bin:$$PATH"
GLSLANG_CC ?= gcc
GLSLANG_CXX ?= g++
MESA_TARGET_FLAGS ?= $(if $(filter Windows_NT,$(OS)),-L$(NYBLER_MINGW_LIB) -fuse-ld=bfd,)
MESON_ENV = CC="$(MESA_CC)" CXX="$(MESA_CXX)" \
	CFLAGS="$(MESA_TARGET_FLAGS)" CXXFLAGS="$(MESA_TARGET_FLAGS)" \
	$(NYBLER_PATH_ENV)
else
NYBLER_TARGET_FLAGS ?=
NYBLER_PATH_ENV = PATH="$(PREFIX)/bin:$$PATH"
GLSLANG_CC ?= $(NYBLER_CC)
GLSLANG_CXX ?= $(NYBLER_CXX)
MESON_ENV = $(NYBLER_PATH_ENV)
endif

ROOT := $(abspath ..)
LEXBOR_SRC := $(ROOT)/third_party/lexbor
TINT_SRC := $(ROOT)/third_party/tint
MESA_SRC := $(ROOT)/third_party/mesa
GLSLANG_SRC := $(TINT_SRC)/third_party/vulkan-deps/glslang/src

LEXBOR_BUILD := $(CURDIR)/$(BUILD)/lexbor
TINT_BUILD := $(CURDIR)/$(BUILD)/tint
TINT_ABSL_BUILD := $(TINT_BUILD)/third_party/abseil
MESA_BUILD := $(CURDIR)/$(BUILD)/mesa
GLSLANG_BUILD := $(CURDIR)/$(BUILD)/glslang
ifeq ($(OS),Windows_NT)
GLSLANG_VALIDATOR := $(GLSLANG_BUILD)/StandAlone/glslangValidator.exe
else
GLSLANG_VALIDATOR := $(GLSLANG_BUILD)/StandAlone/glslangValidator
endif
NYBLER_BUILD := $(CURDIR)/$(BUILD)/nybler
DEPOT_TOOLS ?= $(CURDIR)/$(BUILD)/depot_tools
STAMP_DIR := $(CURDIR)/$(BUILD)/stamps

LEXBOR_STAMP := $(STAMP_DIR)/lexbor.stamp
TINT_STAMP := $(STAMP_DIR)/tint.stamp
GLSLANG_STAMP := $(STAMP_DIR)/glslang.stamp
WINFLEXBISON_STAMP := $(STAMP_DIR)/winflexbison.stamp
MESA_STAMP := $(STAMP_DIR)/mesa.stamp
NYBLER_STAMP := $(STAMP_DIR)/nybler.stamp
NYBLER_SOURCES := $(wildcard $(CURDIR)/src/*.c $(CURDIR)/src/*.cpp \
	$(CURDIR)/src/*.h $(CURDIR)/include/nybler/*.h $(CURDIR)/CMakeLists.txt)

WINFLEXBISON_VERSION ?= 2.5.25
WINFLEXBISON_URL ?= https://github.com/lexxmark/winflexbison/releases/download/v$(WINFLEXBISON_VERSION)/win_flex_bison-$(WINFLEXBISON_VERSION).zip

# Meson's release preset overwrites -Doptimization with -O3 even when the
# option is passed on the command line. Use the plain preset so MESA_ARGS can
# actually choose the optimization level for this local software build.
MESA_BUILDTYPE ?= plain
MESA_ARGS ?= -Dllvm=disabled -Dgallium-drivers=softpipe -Dvulkan-drivers=[] -Degl=enabled -Dgles1=disabled -Dgles2=enabled -Doptimization=0

DOM_SMOKE_BIN := $(CURDIR)/$(BUILD)/dom-smoke$(if $(filter Windows_NT,$(OS)),.exe,)
LAYOUT_SMOKE_BIN := $(CURDIR)/$(BUILD)/layout-smoke$(if $(filter Windows_NT,$(OS)),.exe,)
SHADER_SMOKE_BIN := $(CURDIR)/$(BUILD)/shader-smoke$(if $(filter Windows_NT,$(OS)),.exe,)
RESOURCES_SMOKE_BIN := $(CURDIR)/$(BUILD)/resources-smoke$(if $(filter Windows_NT,$(OS)),.exe,)
MESA_SMOKE_BIN := $(CURDIR)/$(BUILD)/mesa-smoke$(if $(filter Windows_NT,$(OS)),.exe,)
PAINT_SMOKE_BIN := $(CURDIR)/$(BUILD)/paint-smoke$(if $(filter Windows_NT,$(OS)),.exe,)
ifeq ($(OS),Windows_NT)
DOM_SMOKE_OBJ := $(CURDIR)/$(BUILD)/dom-smoke.obj
LAYOUT_SMOKE_OBJ := $(CURDIR)/$(BUILD)/layout-smoke.obj
SHADER_SMOKE_OBJ := $(CURDIR)/$(BUILD)/shader-smoke.obj
RESOURCES_SMOKE_OBJ := $(CURDIR)/$(BUILD)/resources-smoke.obj
MESA_SMOKE_OBJ := $(CURDIR)/$(BUILD)/mesa-smoke.obj
PAINT_SMOKE_OBJ := $(CURDIR)/$(BUILD)/paint-smoke.obj
LEXBOR_LIB ?= $(PREFIX)/lib/lexbor_static.lib
NYBLER_LIB ?= $(NYBLER_BUILD)/nybler.lib
NYBLER_TINT_LIBS = $(wildcard $(PREFIX)/lib/tint*.lib)
NYBLER_ABSL_LIBS = $(wildcard $(PREFIX)/lib/absl*.lib)
# clang++ on Windows uses the MSVC ABI and lld-link by default, so GNU linker
# flags such as -static-libstdc++ and --start-group are ignored there.
NYBLER_CXX_LINK_FLAGS ?=
NYBLER_LINK_TINT = $(NYBLER_TINT_LIBS) $(NYBLER_ABSL_LIBS) $(NYBLER_ABSL_LIBS)
else
DOM_SMOKE_OBJ := $(CURDIR)/$(BUILD)/dom-smoke.o
LAYOUT_SMOKE_OBJ := $(CURDIR)/$(BUILD)/layout-smoke.o
SHADER_SMOKE_OBJ := $(CURDIR)/$(BUILD)/shader-smoke.o
RESOURCES_SMOKE_OBJ := $(CURDIR)/$(BUILD)/resources-smoke.o
MESA_SMOKE_OBJ := $(CURDIR)/$(BUILD)/mesa-smoke.o
PAINT_SMOKE_OBJ := $(CURDIR)/$(BUILD)/paint-smoke.o
LEXBOR_LIB ?= $(PREFIX)/lib/liblexbor_static.a
NYBLER_LIB ?= $(NYBLER_BUILD)/libnybler.a
NYBLER_TINT_LIBS = $(wildcard $(PREFIX)/lib/libtint*.a)
NYBLER_ABSL_LIBS = $(wildcard $(PREFIX)/lib/libabsl*.a)
NYBLER_CXX_LINK_FLAGS ?=
NYBLER_LINK_TINT = -Wl,--start-group $(NYBLER_TINT_LIBS) $(NYBLER_ABSL_LIBS) -Wl,--end-group
endif

.PHONY: all deps lexbor tint glslang mesa nybler dom-smoke layout-smoke shader-smoke resources-smoke mesa-smoke paint-smoke check clean

all: deps nybler

deps: lexbor tint glslang mesa

lexbor: $(LEXBOR_STAMP)
tint: $(TINT_STAMP)
glslang: $(GLSLANG_STAMP)
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
	$(PYTHON) $(CURDIR)/tools/copy_abseil_libs.py $(TINT_ABSL_BUILD) $(PREFIX)/lib
	$(CMAKE) -E touch $@

$(GLSLANG_STAMP):
	$(CMAKE) -E make_directory $(STAMP_DIR)
	$(NYBLER_PATH_ENV) $(CMAKE) -S $(GLSLANG_SRC) -B $(GLSLANG_BUILD) -G $(CMAKE_GENERATOR) \
		-DCMAKE_C_COMPILER=$(GLSLANG_CC) \
		-DCMAKE_CXX_COMPILER=$(GLSLANG_CXX) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX) \
		-DBUILD_EXTERNAL=OFF \
		-DBUILD_SHARED_LIBS=OFF \
		-DENABLE_OPT=OFF \
		-DENABLE_CTEST=OFF \
		-DENABLE_GLSLANG_JS=OFF \
		-DENABLE_HLSL=OFF \
		-DENABLE_SPVREMAPPER=OFF \
		-DSKIP_GLSLANG_INSTALL=OFF
	$(NYBLER_PATH_ENV) $(CMAKE) --build $(GLSLANG_BUILD) --target glslang-standalone --parallel $(NYBLER_JOBS)
	$(NYBLER_PATH_ENV) $(CMAKE) --install $(GLSLANG_BUILD)
	$(CMAKE) -E make_directory $(PREFIX)/bin
	$(CMAKE) -E copy_if_different $(GLSLANG_VALIDATOR) $(PREFIX)/bin/
	$(CMAKE) -E touch $@

$(WINFLEXBISON_STAMP):
	$(CMAKE) -E make_directory $(STAMP_DIR)
	$(CMAKE) -E make_directory $(NYBLER_TMP)
	$(POWERSHELL) -NoProfile -Command "Invoke-WebRequest -Uri '$(WINFLEXBISON_URL)' -OutFile '$(NYBLER_TMP)/win_flex_bison-$(WINFLEXBISON_VERSION).zip'"
	$(CMAKE) -E remove_directory $(NYBLER_TMP)/winflexbison
	$(POWERSHELL) -NoProfile -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('$(NYBLER_TMP)/win_flex_bison-$(WINFLEXBISON_VERSION).zip', '$(NYBLER_TMP)/winflexbison')"
	$(CMAKE) -E make_directory $(PREFIX)/bin
	$(CMAKE) -E copy_if_different $(NYBLER_TMP)/winflexbison/win_flex.exe $(PREFIX)/bin/
	$(CMAKE) -E copy_if_different $(NYBLER_TMP)/winflexbison/win_bison.exe $(PREFIX)/bin/
	$(CMAKE) -E copy_if_different $(NYBLER_TMP)/winflexbison/FlexLexer.h $(PREFIX)/bin/
	$(CMAKE) -E copy_directory_if_different $(NYBLER_TMP)/winflexbison/data $(PREFIX)/bin/data
	$(CMAKE) -E touch $@

$(MESA_STAMP): $(GLSLANG_STAMP) $(if $(filter Windows_NT,$(OS)),$(WINFLEXBISON_STAMP),)
	$(CMAKE) -E make_directory $(STAMP_DIR)
	$(CMAKE) -E make_directory $(NYBLER_TMP)
	$(CMAKE) -E make_directory $(PREFIX)/bin
	$(PY_WRAPPERS)
	$(MESON_ENV) TMPDIR=$(NYBLER_TMP) TMP=$(NYBLER_TMP) TEMP=$(NYBLER_TMP) $(NYBLER_MESON) setup $(MESA_BUILD) $(MESA_SRC) \
		--prefix=$(PREFIX) \
		--buildtype=$(MESA_BUILDTYPE) \
		$(MESA_ARGS)
	$(if $(filter Windows_NT,$(OS)),$(POWERSHELL) -NoProfile -Command '(Get-Item "$(MESA_BUILD)/meson-private/coredata.dat").LastWriteTime = (Get-Date).AddSeconds(-2)',)
	$(MESON_ENV) TMPDIR=$(NYBLER_TMP) TMP=$(NYBLER_TMP) TEMP=$(NYBLER_TMP) $(NYBLER_MESON) compile -C $(MESA_BUILD) -j $(NYBLER_JOBS)
	$(MESON_ENV) TMPDIR=$(NYBLER_TMP) TMP=$(NYBLER_TMP) TEMP=$(NYBLER_TMP) $(NYBLER_MESON) install -C $(MESA_BUILD)
	$(CMAKE) -E touch $@

$(NYBLER_STAMP): $(LEXBOR_STAMP) $(TINT_STAMP) $(NYBLER_SOURCES)
	$(CMAKE) -E make_directory $(STAMP_DIR)
	$(CMAKE) -S $(CURDIR) -B $(NYBLER_BUILD) -G $(CMAKE_GENERATOR) \
		-DCMAKE_C_COMPILER=$(NYBLER_CC) \
		-DCMAKE_CXX_COMPILER=$(NYBLER_CXX) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_PREFIX_PATH=$(PREFIX) \
		-DLEXBOR_ROOT=$(PREFIX) \
		-DTINT_ROOT=$(PREFIX) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX)
	$(CMAKE) --build $(NYBLER_BUILD) --config $(BUILD_TYPE) --parallel $(NYBLER_JOBS)
	$(CMAKE) --install $(NYBLER_BUILD) --config $(BUILD_TYPE)
	$(CMAKE) -E touch $@

dom-smoke: nybler lexbor
	$(CMAKE) -E make_directory $(CURDIR)/$(BUILD)
	$(NYBLER_CC) -std=c11 -DLEXBOR_STATIC \
		-I$(CURDIR)/include -I$(PREFIX)/include \
		-c -o $(DOM_SMOKE_OBJ) $(CURDIR)/tests/dom_smoke.c
	$(NYBLER_CXX) $(NYBLER_CXX_LINK_FLAGS) \
		-o $(DOM_SMOKE_BIN) $(DOM_SMOKE_OBJ) \
		$(NYBLER_LIB) $(LEXBOR_LIB) $(NYBLER_LINK_TINT)
	$(NYBLER_PATH_ENV) $(DOM_SMOKE_BIN)

layout-smoke: nybler lexbor
	$(CMAKE) -E make_directory $(CURDIR)/$(BUILD)
	$(NYBLER_CC) -std=c11 -DLEXBOR_STATIC \
		-I$(CURDIR)/include -I$(PREFIX)/include \
		-c -o $(LAYOUT_SMOKE_OBJ) $(CURDIR)/tests/layout_smoke.c
	$(NYBLER_CXX) $(NYBLER_CXX_LINK_FLAGS) \
		-o $(LAYOUT_SMOKE_BIN) $(LAYOUT_SMOKE_OBJ) \
		$(NYBLER_LIB) $(LEXBOR_LIB) $(NYBLER_LINK_TINT)
	$(NYBLER_PATH_ENV) $(LAYOUT_SMOKE_BIN)

shader-smoke: nybler lexbor
	$(CMAKE) -E make_directory $(CURDIR)/$(BUILD)
	$(NYBLER_CC) -std=c11 \
		-I$(CURDIR)/include \
		-c -o $(SHADER_SMOKE_OBJ) $(CURDIR)/tests/shader_smoke.c
	$(NYBLER_CXX) $(NYBLER_CXX_LINK_FLAGS) \
		-o $(SHADER_SMOKE_BIN) $(SHADER_SMOKE_OBJ) \
		$(NYBLER_LIB) $(LEXBOR_LIB) $(NYBLER_LINK_TINT)
	$(NYBLER_PATH_ENV) $(SHADER_SMOKE_BIN)

resources-smoke: nybler lexbor
	$(CMAKE) -E make_directory $(CURDIR)/$(BUILD)
	$(NYBLER_CC) -std=c11 \
		-I$(CURDIR)/include \
		-c -o $(RESOURCES_SMOKE_OBJ) $(CURDIR)/tests/resources_smoke.c
	$(NYBLER_CXX) $(NYBLER_CXX_LINK_FLAGS) \
		-o $(RESOURCES_SMOKE_BIN) $(RESOURCES_SMOKE_OBJ) \
		$(NYBLER_LIB) $(LEXBOR_LIB) $(NYBLER_LINK_TINT)
	$(NYBLER_PATH_ENV) $(RESOURCES_SMOKE_BIN)

mesa-smoke: nybler lexbor
	$(CMAKE) -E make_directory $(CURDIR)/$(BUILD)
	$(NYBLER_CC) -std=c11 \
		-I$(CURDIR)/include \
		-c -o $(MESA_SMOKE_OBJ) $(CURDIR)/tests/mesa_smoke.c
	$(NYBLER_CXX) $(NYBLER_CXX_LINK_FLAGS) \
		-o $(MESA_SMOKE_BIN) $(MESA_SMOKE_OBJ) \
		$(NYBLER_LIB) $(LEXBOR_LIB) $(NYBLER_LINK_TINT)
	$(NYBLER_PATH_ENV) $(MESA_SMOKE_BIN)

paint-smoke: nybler lexbor
	$(CMAKE) -E make_directory $(CURDIR)/$(BUILD)
	$(NYBLER_CC) -std=c11 -DLEXBOR_STATIC \
		-I$(CURDIR)/include -I$(PREFIX)/include \
		-c -o $(PAINT_SMOKE_OBJ) $(CURDIR)/tests/paint_smoke.c
	$(NYBLER_CXX) $(NYBLER_CXX_LINK_FLAGS) \
		-o $(PAINT_SMOKE_BIN) $(PAINT_SMOKE_OBJ) \
		$(NYBLER_LIB) $(LEXBOR_LIB) $(NYBLER_LINK_TINT)
	$(NYBLER_PATH_ENV) $(PAINT_SMOKE_BIN)

check: nybler dom-smoke layout-smoke shader-smoke resources-smoke mesa-smoke paint-smoke
	@echo "NybleR C API version: $$($(CMAKE) -P $(CURDIR)/cmake/print-version.cmake 2>/dev/null || echo unversioned)"

clean:
	$(CMAKE) -E remove_directory $(CURDIR)/$(BUILD)
