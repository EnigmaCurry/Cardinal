# Cardinal — native GUI dev targets (fixed-rack branch).
#
# The `native` recipes use the FHS build shell from ./flake.nix, which
# provides gcc/glibc, OpenGL, X11, alsa, jack, liblo, freetype etc.
# `nix run .#` enters the sandbox; `nix run .# -- <cmd>` runs one-shot.

set shell := ["bash", "-euo", "pipefail", "-c"]

default:
    @just --list

# Build bin/Cardinal (JACK standalone, native GUI).
build:
    nix run .# -- make -j"$(nproc)" jack

# Launch the native GUI. Assumes `just build` already ran.
run:
    @if [ ! -x bin/Cardinal ]; then \
        echo "!!! bin/Cardinal not built yet — run 'just build' first"; \
        exit 1; \
    fi
    ./bin/Cardinal

# Wipe stale mixed wasm/native artifacts. Run this when switching between
# a wasm build and the native GUI build — cached object files and archive
# libraries from one toolchain are wasm-ld-hostile to the other.
clean-native:
    rm -rf build/rack build/plugins build/Cardinal
    rm -rf carla/build dpf/build
    rm -rf deps/surge-build
    find deps/aubio -name "*.o" -delete
    rm -f deps/aubio/libaubio.a
    rm -f src/rack.a src/rack-headless.a
    rm -f plugins/plugins.a plugins/plugins-headless.a
    rm -f plugins/plugins-mini.a plugins/plugins-mini-headless.a
    # Rack submodule dep libs. The native `just build` invokes the dep
    # Makefile with the host toolchain, replacing whatever the wasm flow
    # put here (including the libarchive stub the outer web-demo Justfile
    # compiles via emcc). Nuking the .a/.la lets both flows regenerate
    # fresh via their own build recipes.
    rm -f src/Rack/dep/lib/*.a src/Rack/dep/lib/*.la
    # CMake refuses to switch toolchains once configured, but zstd's
    # `build/cmake/` is part of the extracted tarball (not a build output)
    # and libarchive's `build/` is similar — the outer deps/Makefile's
    # .stamp-patched files record that extraction was done, so nuking the
    # subdir without also removing the stamp leaves the build unable to
    # `cd build/cmake`. Kill both the stamp and only the CMake *cache*
    # (CMakeCache.txt + CMakeFiles/) — leaves source-tree layout intact.
    rm -f src/Rack/dep/zstd-1.4.5/.stamp-patched
    rm -f src/Rack/dep/libarchive-3.4.3/.stamp-patched
    -find src/Rack/dep/zstd-1.4.5 src/Rack/dep/libarchive-3.4.3 \
        \( -name "CMakeCache.txt" -o -name "cmake_install.cmake" -o -name "Makefile" \) \
        -delete 2>/dev/null
    -find src/Rack/dep/zstd-1.4.5 src/Rack/dep/libarchive-3.4.3 \
        -type d -name "CMakeFiles" -exec rm -rf {} + 2>/dev/null
    # Autotools-built deps leave .o + .lo (libtool wrapper) + .Plo (dep info)
    # files in-tree. Nuking only .o leaves libtool thinking .lo is up to date,
    # so the next build skips recompilation and then the link step fails
    # trying to `ar` an .o that no longer exists.
    -find src/Rack/dep/jansson-2.12 src/Rack/dep/libsamplerate-0.1.9 src/Rack/dep/speexdsp \
        \( -name "*.o" -o -name "*.lo" -o -name "*.la" -o -name "*.Plo" -o -name "*.Tpo" \) -delete 2>/dev/null
    -find src/Rack/dep/jansson-2.12 src/Rack/dep/libsamplerate-0.1.9 src/Rack/dep/speexdsp \
        -type d -name ".libs" -exec rm -rf {} + 2>/dev/null
    @echo ">>> Native build tree cleaned. Next wasm or native build will be a full rebuild."
