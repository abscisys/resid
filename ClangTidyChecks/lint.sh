#!/usr/bin/env bash
#
# lint.sh -- build synthaxes-clang-tidy, then lint first-party code with it.
#
# Always builds the tool first (fast: it links prebuilt LLVM static libs), so
# the checks and the code being checked can never drift apart. Nothing here runs
# during a normal build; linting is on demand.
#
#   ./lint.sh                 lint the converted layers
#   ./lint.sh libs/preset     lint one library
#   ./lint.sh --fix           apply the fix-its
#   ./lint.sh --all           include the layers still carrying upstream bodies
#
# WHAT IS LINTED, AND WHY NOT EVERYTHING
#
# Every library that builds for the host: core, dsp, engine, preset, synth and
# and tests. The engine is included even though it is upstream code imported
# verbatim -- it is ours now, and hiding its findings would hide the size of the
# conversion still owed rather than making it smaller.
#
# --target lints the layers that are only built for the Pi: driver/, ui/, FmSynthesizer
# and Main, which only build for the Raspberry Pi and so appear in the ARM
# compile database rather than the host one. clang can analyse them given the
# cross toolchain's C++ headers plus its OWN resource directory -- pointing it
# at GCC's builtin include dir instead makes it choke on GCC's arm_neon.h,
# which uses builtin types clang does not have.
#
# --all lints src/ wholesale against the host database.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"

targets=(libs/core libs/dsp libs/engine libs/preset libs/synth libs/os libs/os-host
         libs/assets libs/control libs/testsupport)
fix=0
bare_metal=0
for arg in "$@"; do
    case "$arg" in
        --fix)  fix=1 ;;
        --all)  targets=(libs kernel) ;;
        --target) bare_metal=1
                  targets=(libs/driver libs/ui libs/os-circle kernel) ;;
        -h|--help) sed -n '3,30p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *)      targets=("$arg") ;;
    esac
done

# --- the tool -------------------------------------------------------------
# Needs an LLVM *development* install; the toolchain-only package has no
# libclangTidy. 21 and 22 both work -- the module registration differs between
# them and SynthaxesTidyModule.cpp handles both.
if [[ -n "${SYNTHAXES_LLVM_PREFIX:-}" ]]; then
    llvm_prefix="$SYNTHAXES_LLVM_PREFIX"
else
    llvm_prefix=""
    for v in 22 21; do
        [[ -d "/usr/lib/llvm-$v/lib/cmake/llvm" ]] && { llvm_prefix="/usr/lib/llvm-$v"; break; }
    done
    [[ -n "$llvm_prefix" ]] || { echo "no LLVM development install found; set SYNTHAXES_LLVM_PREFIX" >&2; exit 1; }
fi

echo "==> Building synthaxes-clang-tidy against $llvm_prefix"

# A build directory configured against a different LLVM is thrown away rather
# than reconfigured. CMake keeps cache entries a new -D does not reach, and the
# result was a tool linking two LLVM majors at once -- which does not fail to
# build, and does not report the same findings as CI.
cache="$here/build-linux/CMakeCache.txt"
if [[ -f "$cache" ]] && ! grep -q "^LLVM_DIR.*=$llvm_prefix/lib/cmake/llvm$" "$cache"; then
    echo "    (configured against another LLVM; starting the tool's build again)"
    rm -rf "$here/build-linux" "$here/bin"
fi

cmake -S "$here" -B "$here/build-linux" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_DIR="$llvm_prefix/lib/cmake/llvm" -DClang_DIR="$llvm_prefix/lib/cmake/clang" >/dev/null
cmake --build "$here/build-linux" >/dev/null
tool="$here/bin/synthaxes-clang-tidy"

# --- the compile database -------------------------------------------------
if [[ "$bare_metal" == 1 ]]; then
    db="$repo/build/RPI4"
    preset=RPI4
else
    db="$repo/build/linux"
    preset=linux
fi
if [[ ! -f "$db/compile_commands.json" ]]; then
    echo "==> Configuring the $preset preset for its compile database"
    ( cd "$repo" && cmake --preset "$preset" >/dev/null )
fi

# --- lint -----------------------------------------------------------------
# The database is GCC's but the tool is clang, so pin clang to the same GCC's
# libstdc++; otherwise it picks a newer one whose headers may be absent and
# every file reports "'cassert' file not found" instead of being analysed.
# Without -header-filter, clang-tidy reports diagnostics ONLY in the .cpp it is
# handed and stays silent about every header that .cpp pulls in. That is how the
# whole include/ tree went unchecked while this script reported zero: the checks
# ran, they just had nothing to say about headers. Scoped to our own headers so
# circle, CMSIS and the toolchain stay quiet.
#
# A header included by several .cpp files is reported once per translation unit,
# so pipe through `sort -u` when counting.
# Headers are checked in BOTH scopes. Without -header-filter clang-tidy reports
# only the .cpp it is handed and stays silent about everything it includes,
# which is how the entire include/ tree went unchecked while this script
# reported zero. Scoped to our own headers so circle, CMSIS and the toolchain
# stay quiet.
#
# A header included by several .cpp files is reported once per translation unit,
# so pipe through `sort -u` when counting.
extra=(--header-filter='synthaxes/hw/fm/')
if [[ "$bare_metal" == 1 ]]; then
    tc="$repo/.external/toolchain/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-elf"
    tv=13.3.1
    res="$(ls -d "$llvm_prefix"/lib/clang/*/include 2>/dev/null | head -1)"
    extra+=(--extra-arg=--target=aarch64-none-elf
            --extra-arg="-isystem$res"
            --extra-arg="-isystem$tc/aarch64-none-elf/include/c++/$tv"
            --extra-arg="-isystem$tc/aarch64-none-elf/include/c++/$tv/aarch64-none-elf"
            --extra-arg="-isystem$tc/aarch64-none-elf/include")
elif command -v g++ >/dev/null 2>&1; then
    gcc_dir="$(dirname "$(g++ -print-file-name=crtbegin.o)")"
    [[ -d "$gcc_dir" ]] && extra+=(--extra-arg="--gcc-install-dir=$gcc_dir")
fi
[[ "$fix" == 1 ]] && extra+=(--fix)

cd "$repo"
files=()
for t in "${targets[@]}"; do
    while IFS= read -r f; do files+=("$f"); done < <(
        if [[ -f "$t" ]]; then echo "$t"; else
            find "$t" -name '*.cpp' -o -name '*.c' 2>/dev/null | sort
        fi)
done

# Keep only files the build actually compiles. A source with no entry in the
# database is not a translation unit -- libs/preset/src/Voices.c is the DX7 factory
# ROM, #included into SysExFileLoader.cpp -- and clang-tidy would fall back to
# invented arguments and report parse errors about the fallback rather than
# about the code.
if [[ ${#files[@]} -gt 0 ]]; then
    mapfile -t files < <(
        printf '%s\n' "${files[@]}" |
        python3 -c '
import json, os, sys
db = os.path.join(sys.argv[1], "compile_commands.json")
known = {os.path.realpath(e["file"]) for e in json.load(open(db))}
for line in sys.stdin.read().split():
    if os.path.realpath(line) in known:
        print(line)
' "$db")
fi

if [[ ${#files[@]} -eq 0 ]]; then
    echo "no sources to lint under: ${targets[*]}"
    exit 0
fi

echo "==> Linting ${#files[@]} files"
status=0
for f in "${files[@]}"; do
    if ! "$tool" --quiet -p "$db" "${extra[@]}" "$f" 2>&1 | grep -E 'warning:|error:' ; then
        printf '  %-40s clean\n' "$f"
    else
        status=1
    fi
done
exit $status
