/*
 * Cardinal WasmDSP — headless DSP-only wasm entry point.
 *
 * Sidesteps DPF's Plugin/UI class hierarchy (which doesn't compile clean
 * under HEADLESS+WASM). Instead constructs a CardinalPluginContext + a
 * rack::engine::Engine directly, wires them together, and exposes a
 * small C ABI for the JS side to drive.
 *
 * Public C API (see EMSCRIPTEN_KEEPALIVE):
 *   cardinal_init(sample_rate, block_size)      -> 0 on success
 *   cardinal_load_patch_json(json_utf8)         -> 0 on success
 *   cardinal_reset()                            -> clears the engine
 *   cardinal_process(frames)                    -> ticks the engine one block
 *   cardinal_get_input_buffer(ch)               -> ptr to input float[block]
 *   cardinal_get_output_buffer(ch)              -> ptr to output float[block]
 *   cardinal_get_block_size()                   -> configured block size
 *
 * JS-side flow per AudioWorklet quantum:
 *   1. Copy input into cardinal_get_input_buffer(0..N)
 *   2. cardinal_process(N)
 *   3. Copy cardinal_get_output_buffer(0..N) into worklet output
 */

#include <emscripten/emscripten.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

#include <jansson.h>

// Rack public headers — DSP-only slice.
#include <rack.hpp>
#include <context.hpp>
#include <engine/Engine.hpp>
#include <plugin.hpp>

// Cardinal's extended context, required because plugins-mini-headless.a
// modules (esp. HostAudio) `static_cast<CardinalPluginContext*>(APP)`
// and read fields like bufferSize / processCounter / dataIns / dataOuts.
#include "DistrhoPluginInfo.h"
#include "plugincontext.hpp"

// -----------------------------------------------------------------------
// Global engine state. Lives for the lifetime of the wasm module.

namespace {

CardinalPluginContext* g_pcontext = nullptr;
rack::engine::Engine*  g_engine   = nullptr;

std::vector<float*> g_ins;   // channel-major float[frames] each
std::vector<float*> g_outs;
int  g_block_size = 0;

// MIDI event queue. JS pushes events between cardinal_process() calls;
// cardinal_process moves them into pcontext just before stepBlock so
// HostMIDI can dispatch them. Cardinal's MidiEvent is small (frame,
// size, up to 4 bytes inline, no dataExt for MVP).
std::vector<CardinalDISTRHO::MidiEvent> g_midi_queue;

// The audio ports Cardinal exposes to a patch. plugins-mini-headless.a's
// HostAudio module reads/writes through CardinalPluginContext::dataIns /
// dataOuts. We manage the underlying storage.
const float* const* asConstPP(std::vector<float*>& v) {
    // Reinterpret vector<float*> memory as (const float* const*). Fine
    // as long as we never write through the returned pointers; we don't.
    return reinterpret_cast<const float* const*>(v.data());
}

} // namespace

// Cardinal's plugin registration entry point. Defined inside
// plugins-mini-headless.a — registers every module we can address by name
// in a .vcv/.json patch.
namespace rack { namespace plugin { void initStaticPlugins(); } }

// Rack expects these to be provided by the host program. In stock Cardinal
// they live in src/CardinalMini/common.cpp; we define them here to avoid
// dragging in the DPF `getPluginFormatName()` dependency that file needs.
namespace rack {
    const std::string APP_NAME = "CardinalWasmDSP";
    const std::string APP_EDITION = "wasm-dsp";
    const std::string APP_EDITION_NAME = "Headless WebAssembly DSP";
    const std::string APP_VERSION_MAJOR = "2";
    const std::string APP_VERSION = "2.4.1";
    const std::string APP_OS = "lin";       // ARCH_LIN is what we #define
    const std::string API_URL = "";

    // Rack's Exception ctor lives in the same TU as APP_VERSION upstream;
    // provide it here rather than dragging in another source file.
    Exception::Exception(const char* format, ...) {
        va_list args;
        va_start(args, format);
        msg = string::fV(format, args);
        va_end(args);
    }
}

// plugins-mini-headless.a was compiled with -DDISTRHO_OS_WASM, which
// causes some resource-loading code to route through fopen_wasm() (a
// Cardinal-internal shim that chmods before fopen). We provide it here.
//
// CRITICAL: Cardinal/include/common.hpp does `#define fopen fopen_wasm`,
// which we pull in via <rack.hpp>. So the `std::fopen(...)` call below
// would expand to `std::fopen_wasm(...)` at preprocess time — infinite
// self-recursion. Upstream src/CardinalMini/common.cpp undefines fopen
// before the definition; we do the same.
#include <sys/stat.h>
#undef fopen
extern "C" FILE* fopen_wasm(const char* filename, const char* mode) {
    chmod(filename, 0777);
    return std::fopen(filename, mode);
}

// -----------------------------------------------------------------------
// C ABI

extern "C" {

#define TRACE(msg) do { std::fprintf(stderr, "[cardinal] %s\n", msg); std::fflush(stderr); } while (0)

EMSCRIPTEN_KEEPALIVE
int cardinal_init(int sample_rate, int block_size)
{
    if (g_pcontext != nullptr) return 0;
    if (sample_rate <= 0 || block_size <= 0) return -1;

    TRACE("init: constructing CardinalPluginContext");
    g_pcontext = new CardinalPluginContext(nullptr);
    g_pcontext->sampleRate = static_cast<double>(sample_rate);
    g_pcontext->bufferSize = static_cast<uint32_t>(block_size);
    g_pcontext->playing = true;

    TRACE("init: constructing rack::engine::Engine");
    g_engine = new rack::engine::Engine;
    TRACE("init: engine->setSampleRate");
    g_engine->setSampleRate(static_cast<float>(sample_rate));
    g_pcontext->engine = g_engine;

    TRACE("init: contextSet");
    rack::contextSet(g_pcontext);

    // Cardinal's custom/asset.cpp returns "../../plugins/X/plugin.json"
    // when systemDir is unset (default). Emscripten's MEMFS treats the
    // leading `../..` literally at root and can't find our embedded
    // manifests at /plugins/X. Establish a nested CWD so the relative
    // path resolves correctly.
    ::mkdir("/wasm", 0777);
    ::mkdir("/wasm/build", 0777);
    if (::chdir("/wasm/build") != 0) {
        std::fprintf(stderr, "[cardinal] chdir(/wasm/build) failed\n");
    }
    TRACE("init: cwd set so ../.. == /");

    TRACE("init: initStaticPlugins() — this is where things usually get slow");
    try {
        rack::plugin::initStaticPlugins();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[cardinal] initStaticPlugins threw: %s\n", e.what());
        return -2;
    }
    TRACE("init: initStaticPlugins done");

    g_block_size = block_size;
    g_ins.resize(CARDINAL_NUM_AUDIO_INPUTS);
    g_outs.resize(CARDINAL_NUM_AUDIO_OUTPUTS);
    for (auto& p : g_ins)  p = new float[block_size]();
    for (auto& p : g_outs) p = new float[block_size]();
    g_pcontext->dataIns  = asConstPP(g_ins);
    g_pcontext->dataOuts = g_outs.data();

    TRACE("init: done");
    return 0;
}
#undef TRACE

EMSCRIPTEN_KEEPALIVE
int cardinal_load_patch_json(const char* json_str)
{
    if (g_engine == nullptr || json_str == nullptr) return -1;

    // Clearing the engine while it's being stepped from another thread
    // would race, but we're single-threaded in wasm.
    g_engine->clear();

    json_error_t err;
    json_t* root = json_loads(json_str, 0, &err);
    if (root == nullptr) {
        std::fprintf(stderr, "cardinal: json parse failed at line %d: %s\n",
                     err.line, err.text);
        return -2;
    }
    try {
        g_engine->fromJson(root);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "cardinal: engine->fromJson threw: %s\n", e.what());
        json_decref(root);
        return -3;
    }
    json_decref(root);
    return 0;
}

EMSCRIPTEN_KEEPALIVE
void cardinal_reset(void)
{
    if (g_engine != nullptr) g_engine->clear();
}

EMSCRIPTEN_KEEPALIVE
void cardinal_process(int frames)
{
    if (g_engine == nullptr) return;
    if (frames <= 0 || frames > g_block_size) return;

    // HostAudio writes to dataOuts with `+=` (accumulate). Rack's real
    // host zeroes the buffers before each block; we do the same.
    for (auto* p : g_outs) std::memset(p, 0, sizeof(float) * frames);

    // Wire the queued MIDI events (pushed by JS since last block) into
    // the plugin context for HostMIDI to consume during stepBlock.
    if (!g_midi_queue.empty()) {
        g_pcontext->midiEvents     = g_midi_queue.data();
        g_pcontext->midiEventCount = static_cast<uint32_t>(g_midi_queue.size());
    } else {
        g_pcontext->midiEvents     = nullptr;
        g_pcontext->midiEventCount = 0;
    }

    g_pcontext->processCounter += 1;
    g_engine->stepBlock(frames);
    g_pcontext->frame += static_cast<uint64_t>(frames);

    g_midi_queue.clear();
}

// Push one MIDI event into the queue for the NEXT cardinal_process()
// call. frame_offset is the sub-block frame the event should fire on
// (0 = start of block). Only up to 3 raw bytes are supported (fits
// note-on/off, CC, pitch bend, program change, aftertouch). SysEx would
// need dataExt handling — skipped for MVP.
EMSCRIPTEN_KEEPALIVE
void cardinal_push_midi(int frame_offset, int size, int b0, int b1, int b2)
{
    if (size < 1 || size > 3) return;
    CardinalDISTRHO::MidiEvent ev;
    ev.frame   = static_cast<uint32_t>(frame_offset < 0 ? 0 : frame_offset);
    ev.size    = static_cast<uint32_t>(size);
    ev.data[0] = static_cast<uint8_t>(b0 & 0xff);
    ev.data[1] = static_cast<uint8_t>(b1 & 0xff);
    ev.data[2] = static_cast<uint8_t>(b2 & 0xff);
    ev.data[3] = 0;
    ev.dataExt = nullptr;
    g_midi_queue.push_back(ev);
}

EMSCRIPTEN_KEEPALIVE
float* cardinal_get_input_buffer(int channel)
{
    if (channel < 0 || channel >= static_cast<int>(g_ins.size())) return nullptr;
    return g_ins[channel];
}

EMSCRIPTEN_KEEPALIVE
float* cardinal_get_output_buffer(int channel)
{
    if (channel < 0 || channel >= static_cast<int>(g_outs.size())) return nullptr;
    return g_outs[channel];
}

EMSCRIPTEN_KEEPALIVE
int cardinal_get_block_size(void) { return g_block_size; }

EMSCRIPTEN_KEEPALIVE
int cardinal_get_input_channel_count(void)  { return CARDINAL_NUM_AUDIO_INPUTS; }

EMSCRIPTEN_KEEPALIVE
int cardinal_get_output_channel_count(void) { return CARDINAL_NUM_AUDIO_OUTPUTS; }

} // extern "C"
