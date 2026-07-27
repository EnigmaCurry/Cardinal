// Emscripten JS-library of stub symbols CardinalWasmDSP needs at link
// time but that our headless build doesn't legitimately provide. Each
// entry silences an ERROR_ON_UNDEFINED_SYMBOLS=0 warning from a real
// link-time gap: link succeeds, and if runtime hits the codepath our
// stub is inert instead of trapping.
//
// Wire in via LDFLAGS: --js-library=<path>/stubs.js

mergeInto(LibraryManager.library, {
  // std::__2::__hash_memory(void const*, size_t)
  //
  // libc++ marks this _LIBCPP_HIDE_FROM_ABI (hidden + inline). Normally
  // fully inlined; some codepath in Cardinal keeps a non-inlined ref, and
  // emsdk-3.1.27's libc++.a doesn't export the symbol. We provide a
  // deterministic FNV-1a 32-bit hash — std::hash<T> for POD types just
  // needs consistency within a program run, not any specific algorithm.
  _ZNSt3__213__hash_memoryEPKvm__sig: 'iii',
  _ZNSt3__213__hash_memoryEPKvm: function(ptr, len) {
    var h = 0x811c9dc5;
    for (var i = 0; i < len; i++) {
      h ^= HEAPU8[(ptr + i) >>> 0];
      h = ((h * 0x01000193) >>> 0);
    }
    return h >>> 0;
  },

  // CardinalDISTRHO::Plugin::writeMidiEvent(CardinalDISTRHO::MidiEvent const&)
  //
  // Method on the DPF Plugin base that HostMIDI / HostParametersMap
  // invoke to emit MIDI to the host. CardinalWasmDSP passes nullptr for
  // the DPF Plugin (we don't have one), so the vtable entry isn't there.
  // Headless has no MIDI-out sink; silently drop.
  //
  // Sig 'vii' = void return, (this*, event*) both as i32 pointers.
  _ZN15CardinalDISTRHO6Plugin14writeMidiEventERKNS_9MidiEventE__sig: 'vii',
  _ZN15CardinalDISTRHO6Plugin14writeMidiEventERKNS_9MidiEventE: function(_this, _event) {
    // no-op
  },
});
