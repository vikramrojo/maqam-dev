#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# SLOOP web emulator: web/emu/sloop_web.c (the firmware's own sources + the host HAL) -> build/emu/sloop.wasm.
# Freestanding wasm32 with clang + wasm-ld (macOS: Apple clang has the target, `brew install lld` the linker).
# No imports: the page instantiates it with {}. Needs build/gen (./build.sh, or tools/build.py's generators).
set -e
cd "$(dirname "$0")/../.."
CLANG="${CLANG:-clang}"
WASM_LD="${WASM_LD:-$(command -v wasm-ld || ls /opt/homebrew/bin/wasm-ld /usr/local/bin/wasm-ld 2>/dev/null | head -1)}"
[ -n "$WASM_LD" ] || { echo "wasm-ld not found (brew install lld)"; exit 1; }
[ -f build/gen/felucca_tables.h ] || { echo "build/gen missing: run ./build.sh first"; exit 1; }
mkdir -p build/emu
OUT=build/emu/sloop.wasm
"$CLANG" --target=wasm32 -O2 -ffp-contract=off -ffreestanding -fno-builtin -nostdlib -w \
    -fuse-ld="$WASM_LD" -DFELUCCA_HOST -DFELUCCA_WEB \
    -Ibuild/gen -Ifirmware/src -Ifirmware/hal \
    -Wl,--no-entry -Wl,-z,stack-size=524288 -Wl,--initial-memory=8388608 -Wl,--max-memory=8388608 \
    -o "$OUT" web/emu/sloop_web.c
[ -f web/emu/index.html ] && cp web/emu/index.html build/emu/index.html   # build/emu is servable as it is
echo "$OUT: $(wc -c < "$OUT" | tr -d ' ') bytes"
if command -v node >/dev/null 2>&1; then
    node -e 'const m=new WebAssembly.Module(require("fs").readFileSync(process.argv[1]));const i=WebAssembly.Module.imports(m);console.log("imports:",i.length,"exports:",WebAssembly.Module.exports(m).length);process.exit(i.length===0?0:1)' "$OUT" \
        || { echo "FAIL: the module has imports"; exit 1; }
fi
