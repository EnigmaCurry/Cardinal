/*
 * Cardinal WasmDSP — reuses the shared CardinalCommon.cpp implementation
 * with the UI-heavy paths compiled out via CARDINAL_COMMON_DSP_ONLY.
 *
 * This is the same pattern used by src/CardinalMiniSep and
 * src/CardinalLoader — Cardinal already supports this build mode.
 */

#define CARDINAL_COMMON_DSP_ONLY
#include "../CardinalCommon.cpp"
