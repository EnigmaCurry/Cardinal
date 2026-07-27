/*
 * Cardinal WasmDSP — headless DSP-only wasm variant
 * Provides the CARDINAL_VARIANT_* + DISTRHO_* macros that
 * plugincontext.hpp and Cardinal's headers require.
 */

#pragma once

#define CARDINAL_VARIANT_FX     0
#define CARDINAL_VARIANT_LOADER 0
#define CARDINAL_VARIANT_MAIN   0
#define CARDINAL_VARIANT_MINI   1
#define CARDINAL_VARIANT_NATIVE 0
#define CARDINAL_VARIANT_SYNTH  0

#define CARDINAL_NUM_AUDIO_INPUTS  2
#define CARDINAL_NUM_AUDIO_OUTPUTS 2
#define CARDINAL_NUM_PARAMETERS    24

#define DISTRHO_PLUGIN_BRAND "DISTRHO"
#define DISTRHO_PLUGIN_URI   "https://distrho.kx.studio/plugins/cardinal-wasmdsp"
#define DISTRHO_PLUGIN_NAME  "Cardinal WasmDSP"
#define DISTRHO_PLUGIN_LABEL "CardinalWasmDSP"

#define DISTRHO_PLUGIN_HAS_UI             0
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 0
// Mini variant advertises 2 audio + 5 CV ports each way (see the
// kCardinalParameterMini* enum in CardinalPluginContext.hpp — its
// static_assert relates DISTRHO_PLUGIN_NUM_INPUTS to that layout).
#define DISTRHO_PLUGIN_NUM_INPUTS         (CARDINAL_NUM_AUDIO_INPUTS + 5)
#define DISTRHO_PLUGIN_NUM_OUTPUTS        (CARDINAL_NUM_AUDIO_OUTPUTS + 5)
#define DISTRHO_PLUGIN_WANT_STATE         1
#define DISTRHO_PLUGIN_WANT_TIMEPOS       1
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT    1
#define DISTRHO_PLUGIN_WANT_MIDI_OUTPUT   1
#define DISTRHO_PLUGIN_WANT_MIDI_AS_MPE   1
#define DISTRHO_PLUGIN_WANT_FULL_STATE    1
#define DISTRHO_PLUGIN_IS_SYNTH           0
