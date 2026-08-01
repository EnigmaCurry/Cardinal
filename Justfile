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

# Wipe stale mixed wasm/native artifacts. Run this when switching from a wasm-dsp build to native.
clean-native:
    rm -rf build/rack build/plugins build/Cardinal
    rm -rf carla/build dpf/build
    rm -rf deps/surge-build
    find deps/aubio -name "*.o" -delete
    rm -f deps/aubio/libaubio.a
    rm -f src/rack.a src/rack-headless.a
    rm -f plugins/plugins.a plugins/plugins-headless.a
    rm -f plugins/plugins-mini.a plugins/plugins-mini-headless.a
    @echo ">>> Native build tree cleaned. Next 'just build' will be a full rebuild."
