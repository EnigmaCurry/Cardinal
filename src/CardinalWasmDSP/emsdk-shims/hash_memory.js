// Emscripten JS library — provides std::__hash_memory as a JS function.
//
// The symbol std::__2::__hash_memory(void const*, size_t) is declared in
// libc++'s <__hash_memory> header with _LIBCPP_HIDE_FROM_ABI attribute
// (hidden visibility, inline). Normally the compiler inlines all uses.
// Some codepath in Cardinal (or the vendored plugin submodules) takes a
// non-inlinable reference to it, so it ends up as an external symbol at
// link time — and emsdk-3.1.27's libc++.a doesn't export it.
//
// We stub it with a deterministic 32-bit FNV-1a hash. std::hash<T> for
// POD types doesn't require any particular hash function; only that it's
// deterministic within a program run.
//
// Wire this in via LDFLAGS: --js-library=<path>/hash_memory.js

mergeInto(LibraryManager.library, {
  _ZNSt3__213__hash_memoryEPKvm__sig: 'iii',
  _ZNSt3__213__hash_memoryEPKvm: function(ptr, len) {
    var h = 0x811c9dc5;
    for (var i = 0; i < len; i++) {
      h ^= HEAPU8[(ptr + i) >>> 0];
      // FNV prime * h, kept within uint32 via Math.imul-safe pattern
      h = ((h * 0x01000193) >>> 0);
    }
    return h >>> 0;
  },
});
