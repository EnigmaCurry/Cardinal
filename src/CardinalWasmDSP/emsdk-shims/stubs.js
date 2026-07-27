// Emscripten JS-library of stub symbols the CardinalWasmDSP (headless)
// and CardinalMini (UI) wasm builds need at link time but that we don't
// legitimately provide. Combined with -sERROR_ON_UNDEFINED_SYMBOLS=0
// these keep the wasm bootable; runtime hitting a stubbed codepath just
// gets a deterministic no-op / error-code, not a wasm-abort.
//
// Wire in via LDFLAGS: --js-library=<path>/stubs.js

mergeInto(LibraryManager.library, {
  // -----------------------------------------------------------------
  // libc++ / hash_memory
  // -----------------------------------------------------------------

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

  // -----------------------------------------------------------------
  // DPF Plugin methods (headless build has no DPF Plugin instance)
  // -----------------------------------------------------------------

  // CardinalDISTRHO::Plugin::writeMidiEvent(CardinalDISTRHO::MidiEvent const&)
  _ZN15CardinalDISTRHO6Plugin14writeMidiEventERKNS_9MidiEventE__sig: 'vii',
  _ZN15CardinalDISTRHO6Plugin14writeMidiEventERKNS_9MidiEventE: function() { /* no-op */ },

  // handleHostParameterDrag(const CardinalPluginContext*, uint, bool)
  // — declared under !HEADLESS only, but sometimes referenced.
  _Z23handleHostParameterDragPK21CardinalPluginContextjb__sig: 'viii',
  _Z23handleHostParameterDragPK21CardinalPluginContextjb: function() { /* no-op */ },

  // -----------------------------------------------------------------
  // libarchive — Cardinal's Rack UI uses it to load/save .vcv (tar.zst)
  // archives. We don't link libarchive (PawPaw's wasm target doesn't
  // build it, source build fails on arc4random_buf). These stubs make
  // Cardinal's UI Mini boot; its own "Load Patch" / "Save Patch" menu
  // won't work, but interactive patching works. Load .vcv files
  // through the OUTER project's file picker instead — it does the
  // tar.zst extraction in pure JS (fzstd + inline tar parser).
  //
  // Return codes: libarchive uses ARCHIVE_OK=0 for success and
  // ARCHIVE_FATAL=-30 for hard failure.
  // -----------------------------------------------------------------

  _archive_error_string__sig: 'ii',
  _archive_error_string: function() { return 0; /* NULL */ },

  _archive_entry_new__sig: 'i',
  _archive_entry_new: function() { return 0; },
  _archive_entry_free__sig: 'vi',
  _archive_entry_free: function() { /* no-op */ },
  _archive_entry_filetype__sig: 'ii',
  _archive_entry_filetype: function() { return 0; },
  _archive_entry_mode__sig: 'ii',
  _archive_entry_mode: function() { return 0; },
  _archive_entry_pathname__sig: 'ii',
  _archive_entry_pathname: function() { return 0; },
  _archive_entry_sourcepath__sig: 'ii',
  _archive_entry_sourcepath: function() { return 0; },
  _archive_entry_size__sig: 'ii',
  _archive_entry_size: function() { return 0; },
  _archive_entry_set_gid__sig: 'viii',
  _archive_entry_set_gid: function() {},
  _archive_entry_set_gname__sig: 'vii',
  _archive_entry_set_gname: function() {},
  _archive_entry_set_mode__sig: 'vii',
  _archive_entry_set_mode: function() {},
  _archive_entry_set_pathname__sig: 'vii',
  _archive_entry_set_pathname: function() {},
  _archive_entry_set_uid__sig: 'viii',
  _archive_entry_set_uid: function() {},
  _archive_entry_set_uname__sig: 'vii',
  _archive_entry_set_uname: function() {},

  _archive_read_new__sig: 'i',
  _archive_read_new: function() { return 0; },
  _archive_read_free__sig: 'ii',
  _archive_read_free: function() { return 0; },
  _archive_read_close__sig: 'ii',
  _archive_read_close: function() { return 0; },
  _archive_read_next_header__sig: 'iii',
  _archive_read_next_header: function() { return -30; },
  _archive_read_next_header2__sig: 'iii',
  _archive_read_next_header2: function() { return -30; },
  _archive_read_data_block__sig: 'iiiii',
  _archive_read_data_block: function() { return -30; },
  _archive_read_support_format_tar__sig: 'ii',
  _archive_read_support_format_tar: function() { return 0; },
  _archive_read_support_filter_zstd__sig: 'ii',
  _archive_read_support_filter_zstd: function() { return 0; },
  _archive_read_open__sig: 'iiiiii',
  _archive_read_open: function() { return -30; },
  _archive_read_open_filename__sig: 'iiii',
  _archive_read_open_filename: function() { return -30; },

  _archive_read_disk_new__sig: 'i',
  _archive_read_disk_new: function() { return 0; },
  _archive_read_disk_open__sig: 'iii',
  _archive_read_disk_open: function() { return -30; },
  _archive_read_disk_descend__sig: 'ii',
  _archive_read_disk_descend: function() { return 0; },

  _archive_write_new__sig: 'i',
  _archive_write_new: function() { return 0; },
  _archive_write_free__sig: 'ii',
  _archive_write_free: function() { return 0; },
  _archive_write_close__sig: 'ii',
  _archive_write_close: function() { return 0; },
  _archive_write_open__sig: 'iiiiii',
  _archive_write_open: function() { return -30; },
  _archive_write_open_filename__sig: 'iii',
  _archive_write_open_filename: function() { return -30; },
  _archive_write_header__sig: 'iii',
  _archive_write_header: function() { return -30; },
  _archive_write_data__sig: 'iiii',
  _archive_write_data: function() { return -1; },
  _archive_write_data_block__sig: 'iiiii',
  _archive_write_data_block: function() { return -1; },
  _archive_write_finish_entry__sig: 'ii',
  _archive_write_finish_entry: function() { return 0; },
  _archive_write_add_filter_zstd__sig: 'ii',
  _archive_write_add_filter_zstd: function() { return 0; },
  _archive_write_set_format_pax_restricted__sig: 'ii',
  _archive_write_set_format_pax_restricted: function() { return 0; },
  _archive_write_set_bytes_per_block__sig: 'iii',
  _archive_write_set_bytes_per_block: function() { return 0; },
  _archive_write_set_filter_option__sig: 'iiiii',
  _archive_write_set_filter_option: function() { return 0; },
  _archive_write_disk_new__sig: 'i',
  _archive_write_disk_new: function() { return 0; },
  _archive_write_disk_set_options__sig: 'iii',
  _archive_write_disk_set_options: function() { return 0; },

  // -----------------------------------------------------------------
  // GLFW — Cardinal's UI uses GLFW for window/input abstraction. In wasm
  // most of it is provided by Emscripten's GLFW port; clipboard is not.
  // -----------------------------------------------------------------

  _glfwGetClipboardString__sig: 'ii',
  _glfwGetClipboardString: function() { return 0; },
  _glfwSetClipboardString__sig: 'vii',
  _glfwSetClipboardString: function() {},
});
