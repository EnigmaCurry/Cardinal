# Cardinal WasmDSP — headless DSP-only wasm bundle.
#
# Deliberately does NOT include Makefile.cardinal.mk because that pulls
# in DPF's Plugin/UI framework and CardinalPlugin.cpp, which don't
# compile under HEADLESS+WASM. Instead we take Cardinal's already-built
# static libraries (rack-headless.a, plugins-mini-headless.a) as inputs
# and link our own tiny entry point.
#
# Preconditions (satisfied by `just build` at least once beforehand):
#   * ../rack-headless.a
#   * ../../plugins/plugins-mini-headless.a
#   * ../Rack/dep/lib/lib{jansson,samplerate,speexdsp,zstd}.a
#   * PawPaw wasm sysroot at ~/PawPawBuilds/targets/wasm
#   * emsdk on PATH (source deps/PawPaw/local.env wasm)

CARDINAL_ROOT = ../..
BUILD_DIR     = $(CARDINAL_ROOT)/build-wasmdsp
BIN_DIR       = $(CARDINAL_ROOT)/bin-wasmdsp
PAWPAW_TARGET = $(HOME)/PawPawBuilds/targets/wasm

CXX = em++
AR  = emar

# Cardinal / DPF / Rack include paths (extracted from a real headless
# CardinalMini compile — same set, minus DGL and other UI dirs).
INCLUDES  = -I.
INCLUDES += -I$(CARDINAL_ROOT)/include
INCLUDES += -I$(CARDINAL_ROOT)/include/simd-compat
INCLUDES += -I$(CARDINAL_ROOT)/include/linux-compat
INCLUDES += -I$(CARDINAL_ROOT)/src
INCLUDES += -I$(CARDINAL_ROOT)/src/Rack/include
INCLUDES += -I$(CARDINAL_ROOT)/src/Rack/include/dsp
INCLUDES += -I$(CARDINAL_ROOT)/src/Rack/dep/include
INCLUDES += -I$(CARDINAL_ROOT)/src/Rack/dep/filesystem/include
INCLUDES += -I$(CARDINAL_ROOT)/src/Rack/dep/fuzzysearchdatabase/src
INCLUDES += -I$(CARDINAL_ROOT)/src/Rack/dep/nanosvg/src
INCLUDES += -I$(CARDINAL_ROOT)/src/Rack/dep/oui-blendish
INCLUDES += -I$(CARDINAL_ROOT)/src/Rack/dep/pffft
INCLUDES += -I$(CARDINAL_ROOT)/src/Rack/dep/tinyexpr
INCLUDES += -I$(CARDINAL_ROOT)/src/Rack/dep/simde
INCLUDES += -I$(CARDINAL_ROOT)/dpf/distrho
# nanovg.h — Rack's public window/Svg.hpp #includes <nanovg.h>. Even
# under HEADLESS the header must resolve (its symbols are stubbed).
INCLUDES += -I$(CARDINAL_ROOT)/dpf/dgl/src/nanovg
INCLUDES += -I$(PAWPAW_TARGET)/include

DEFINES  = -DUSING_CARDINAL_NOT_RACK
DEFINES += -DHEADLESS
DEFINES += -DCARDINAL_COMMON_DSP_ONLY
DEFINES += -DARCH_LIN
DEFINES += -DPRIVATE=
DEFINES += -DZSTDLIB_VISIBILITY=
DEFINES += -DSIMDE_ACCURACY_PREFERENCE=0
DEFINES += -DSIMDE_FAST_CONVERSION_RANGE
DEFINES += -DSIMDE_FAST_MATH
DEFINES += -DSIMDE_FAST_NANS
DEFINES += -DSIMDE_FAST_ROUND_MODE
DEFINES += -DSIMDE_FAST_ROUND_TIES
DEFINES += -DDISTRHO_NAMESPACE=CardinalDISTRHO
DEFINES += -DDGL_NAMESPACE=CardinalDGL

CXXFLAGS  = -std=gnu++11 -Os -ffast-math -fno-finite-math-only
CXXFLAGS += -fdata-sections -ffunction-sections -fno-strict-aliasing
CXXFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden
CXXFLAGS += -fexceptions
CXXFLAGS += -msimd128
CXXFLAGS += -DNDEBUG
CXXFLAGS += -pipe -MMD -MP
CXXFLAGS += $(INCLUDES) $(DEFINES)

# Exported functions the JS wrapper will call via ccall/cwrap. Kept on
# one line — emcc parses the whole value as a single comma-separated
# argument, and any embedded whitespace becomes a separate emcc arg.
EXPORTS = _malloc,_free,_cardinal_init,_cardinal_load_patch_json,_cardinal_reset,_cardinal_process,_cardinal_get_input_buffer,_cardinal_get_output_buffer,_cardinal_get_block_size,_cardinal_get_input_channel_count,_cardinal_get_output_channel_count,_cardinal_push_midi

RUNTIME = ccall,cwrap,HEAPF32,HEAPU8,stringToNewUTF8,writeArrayToMemory

# Embed each Mini-variant plugin's plugin.json into the wasm's virtual
# filesystem at the exact path asset::pluginManifest() resolves to
# (../../plugins/X/plugin.json → /plugins/X/plugin.json under MEMFS root).
# Without these, StaticPluginLoader bails out early and NO Rack modules
# get registered, so patches load into an empty engine.
EMBED_PLUGINS = Cardinal Fundamental AriaModules AudibleInstruments \
                BogaudioModules MockbaModular surgext EnigmaCurry \
                ImpromptuModular
EMBED_FLAGS   = $(foreach P,$(EMBED_PLUGINS),--embed-file $(abspath $(CARDINAL_ROOT)/plugins/$P/plugin.json)@/plugins/$P/plugin.json)

LDFLAGS  = -O3 -Wl,--gc-sections
LDFLAGS += -fexceptions
LDFLAGS += -sALLOW_MEMORY_GROWTH
LDFLAGS += $(EMBED_FLAGS)
# Cardinal's static data (module descriptions, sample ROMs from Plaits/etc.)
# is ~17 MB. Default 16 MB initial isn't enough; bump to 64 MB. Growth is
# still enabled so patch data doesn't have to fit here.
LDFLAGS += -sINITIAL_MEMORY=67108864
LDFLAGS += -sERROR_ON_UNDEFINED_SYMBOLS=0
LDFLAGS += -sMODULARIZE=1 -sEXPORT_NAME=CardinalWasmDSP
LDFLAGS += -sENVIRONMENT=web,worker
LDFLAGS += -sEXPORTED_FUNCTIONS=$(EXPORTS)
LDFLAGS += -sEXPORTED_RUNTIME_METHODS=$(RUNTIME)
LDFLAGS += --js-library=$(CURDIR)/emsdk-shims/stubs.js

# Static input libraries (all pre-built). Order matters: dependents
# before dependencies, since wasm-ld is a single-pass linker.
DEPS  = $(CARDINAL_ROOT)/plugins/plugins-mini-headless.a
DEPS += $(CARDINAL_ROOT)/src/rack-headless.a
# Surge synthesis engine — needed by SurgeXT modules in plugins-mini.
SURGE = $(CARDINAL_ROOT)/deps/surge-build
DEPS += $(SURGE)/src/common/libsurge-common.a
DEPS += $(SURGE)/src/common/libjuce_dsp_rack_sub.a
DEPS += $(SURGE)/libs/airwindows/libairwindows.a
DEPS += $(SURGE)/libs/eurorack/libeurorack.a
DEPS += $(SURGE)/libs/fmt/libfmt.a
DEPS += $(SURGE)/libs/sqlite-3.23.3/libsqlite.a
DEPS += $(SURGE)/libs/sst/sst-plugininfra/libsst-plugininfra.a
DEPS += $(SURGE)/libs/sst/sst-plugininfra/libs/filesystem/libfilesystem.a
DEPS += $(SURGE)/libs/sst/sst-plugininfra/libs/strnatcmp/libstrnatcmp.a
DEPS += $(SURGE)/libs/sst/sst-plugininfra/libs/tinyxml/libtinyxml.a
# Rack DSP dependencies.
DEPS += $(CARDINAL_ROOT)/src/Rack/dep/lib/libjansson.a
DEPS += $(CARDINAL_ROOT)/src/Rack/dep/lib/libsamplerate.a
DEPS += $(CARDINAL_ROOT)/src/Rack/dep/lib/libspeexdsp.a
DEPS += $(CARDINAL_ROOT)/src/Rack/dep/lib/libzstd.a

OBJS  = $(BUILD_DIR)/CardinalWasmDSP.cpp.o
OBJS += $(BUILD_DIR)/CardinalCommon.cpp.o
# nanovg.c — Rack has static-initialiser `const NVGcolor kFoo = nvgRGBA(...)`
# globals that fire before main. Without nanovg's actual color/math
# helpers linked in, the wasm aborts at init. nanovg.c is a self-contained
# C file; the OpenGL renderer lives elsewhere and stays out of our link.
OBJS += $(BUILD_DIR)/nanovg.c.o

.PHONY: all clean check-deps

all: check-deps $(BIN_DIR)/CardinalWasmDSP.js

check-deps:
	@missing=""; for f in $(DEPS); do \
	    if [ ! -f "$$f" ]; then missing="$$missing\n  - $$f"; fi; \
	done; \
	if [ -n "$$missing" ]; then \
	    echo "!!! Missing prerequisites:$$missing"; \
	    echo "    Run 'just build' at least once to produce these."; \
	    exit 1; \
	fi

$(BUILD_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(BUILD_DIR)
	@echo "  CXX  $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# nanovg.c — Cardinal patched it in a way that needs C++ tag rules
# (nvgCreateImageRaw uses `NVGtexture` without the `enum` prefix that C
# requires). em++ with `-x c++` compiles the same file cleanly and the
# generated code is equivalent for our use.
$(BUILD_DIR)/nanovg.c.o: $(CARDINAL_ROOT)/dpf/dgl/src/nanovg/nanovg.c
	@mkdir -p $(BUILD_DIR)
	@echo "  CXX  nanovg.c (-x c++)"
	@em++ -x c++ -std=gnu++11 -Os -ffast-math -fno-finite-math-only \
	    -fdata-sections -ffunction-sections -fno-strict-aliasing \
	    -fvisibility=hidden -msimd128 -DNDEBUG \
	    -Wno-unused-function -Wno-implicit-function-declaration \
	    -Wno-writable-strings -Wno-narrowing \
	    -I$(CARDINAL_ROOT)/dpf/dgl/src/nanovg \
	    -c $< -o $@

$(BIN_DIR)/CardinalWasmDSP.js: $(OBJS) $(DEPS)
	@mkdir -p $(BIN_DIR)
	@echo "  LINK $@"
	$(CXX) $(OBJS) $(DEPS) $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

-include $(OBJS:.o=.d)
