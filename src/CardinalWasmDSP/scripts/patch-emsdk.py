#!/usr/bin/env python3
"""Idempotently patch emsdk 3.1.27's extract_metadata.py to whitelist only
the wasm feature flags binaryen v111 (the version bundled with emsdk
3.1.27) actually understands.

Background: LLVM 15+ writes newer feature flags (--enable-bulk-memory-opt,
--enable-call-indirect-overlong, ...) into the wasm features section.
emsdk's extract_metadata.py forwards them verbatim to wasm-opt. The
bundled wasm-opt rejects the ones it doesn't know and fails the link.
"""
from __future__ import annotations

import pathlib
import sys

MARKER = "cardinal-web-demo: whitelist v111 features"

# Sourced from `wasm-opt --help | grep -oP '(?<=  )--enable-\S+'` on
# ~/PawPawBuilds/emsdk/upstream/bin/wasm-opt (binaryen v111).
V111_OK = frozenset([
    "--enable-bulk-memory",
    "--enable-exception-handling",
    "--enable-extended-const",
    "--enable-gc",
    "--enable-gc-nn-locals",
    "--enable-memory64",
    "--enable-multi-memories",
    "--enable-multivalue",
    "--enable-mutable-globals",
    "--enable-nontrapping-float-to-int",
    "--enable-reference-types",
    "--enable-relaxed-simd",
    "--enable-sign-ext",
    "--enable-simd",
    "--enable-strings",
    "--enable-tail-call",
    "--enable-threads",
    "--enable-typed-function-references",
])

ANCHOR = (
    "features = [f.replace('--enable-nontrapping-fptoint', "
    "'--enable-nontrapping-float-to-int') for f in features]"
)


def main() -> int:
    target = pathlib.Path.home() / (
        "PawPawBuilds/emsdk/upstream/emscripten/tools/extract_metadata.py"
    )
    if not target.exists():
        print(f"!!! {target} not found — run 'just bootstrap' first.", file=sys.stderr)
        return 1

    src = target.read_text()

    if MARKER in src:
        print(">>> emsdk already patched (whitelist active).")
        return 0

    # Wipe any earlier single-flag patch from a previous iteration.
    if "cardinal-web-demo: strip bulk-memory-opt" in src:
        lines = src.splitlines(keepends=True)
        pruned = []
        skip_next = False
        for line in lines:
            if "cardinal-web-demo: strip bulk-memory-opt" in line:
                skip_next = True
                continue
            if skip_next and "features = [f for f in features if f != '--enable-bulk-memory-opt']" in line:
                skip_next = False
                continue
            pruned.append(line)
        src = "".join(pruned)
        print(">>> Removed previous single-flag patch.")

    if ANCHOR not in src:
        print("!!! anchor line not found in extract_metadata.py — has emsdk changed?", file=sys.stderr)
        return 2

    ok_lines = ",\n        ".join(f"'{name}'" for name in sorted(V111_OK))
    addition = (
        f"\n    # {MARKER}\n"
        f"    _v111_ok = frozenset([\n"
        f"        {ok_lines},\n"
        f"    ])\n"
        f"    features = [f for f in features if f in _v111_ok]"
    )
    target.write_text(src.replace(ANCHOR, ANCHOR + addition))
    print(">>> emsdk patched (whitelist).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
