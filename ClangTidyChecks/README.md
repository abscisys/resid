# synthaxes-clang-tidy

A **custom clang-tidy** for SynthAxesVST3: the stock checks (naming via
`readability-identifier-naming`, plus bugprone/performance/misc) **and** our own
`synthaxes-explicit-this`, in one binary.

`synthaxes-explicit-this` enforces the CLAUDE.md §5 rules that no stock linter can:

| | rule |
|---|---|
| `this->` | member access inside a class must be `this->`-qualified |
| `ClassName::` | static-member access must be `ClassName::`-qualified |

It reports violations **with fix-its** (it inserts the missing qualifier).

## Why this shape

- **A custom binary, not a `--load` plugin.** Loadable clang-tidy plugins do not work on Windows
  (`clang-tidy.exe` exports no symbols for a plugin DLL to bind to). So we build our *own*
  `clang-tidy` by linking the LLVM **development** distribution's static libs (`clangTidyMain`,
  `clangTidy`, the module libs) together with our check. Normal executable → works everywhere.
- **No LLVM source build.** The dev SDK already ships the static libs + headers, so the tool links
  in seconds. (It does need a *development* LLVM install — see below — not the toolchain-only one.)
- **The C++ is 100% platform-agnostic** (pure Clang AST analysis, no OS calls, zero `#ifdef`). The
  only platform variance is in the *build*, isolated in `cmake/<System>.cmake`.

## Layout

```
include/synthaxes/tidy/ThisUsageCheck.h   the check (shared)
src/ThisUsageCheck.cpp                     its logic (shared)
src/SynthaxesTidyModule.cpp                registers the module (shared)
src/main.cpp                               the driver: clangTidyMain + module force-linking (shared)
cmake/Windows.cmake                        DIA-SDK path remap, /GR-
cmake/Linux.cmake                          -fno-rtti
cmake/Darwin.cmake                         -fno-rtti, Homebrew notes
tests/                                     fixture + CTest assertions
bin/                                       the built exe (gitignored)
lint.ps1 / lint.sh                         build the tool, then lint the repo
```

## Prerequisites (an LLVM **development** install)

Pinned to **LLVM 22** on every platform, so the lint verdicts are identical everywhere.

| platform | provides the dev SDK (the `clangTidy*` static libs + `ClangTidyCheck.h`) |
|---|---|
| Windows | the LLVM 22 installer's *full/development* distribution (headers under `include/clang-tidy`, the `clangTidy*.lib`) |
| Linux   | apt.llvm.org: `libclang-22-dev` (carries the clang-tidy static libs + headers) and `clang-tidy-22` (for `run-clang-tidy-22`) |
| macOS   | `brew install llvm` |

## Use

Run the platform script; it **builds the tool first**, then lints. Linting is on-demand only —
it is never part of the normal project build.

```powershell
./lint.ps1                      # Windows: report over dsp/ projects/ ui/ tools/
./lint.ps1 -Filter dsp/PhysiX   # one subtree
./lint.ps1 -Fix                 # apply fix-its
```
```bash
./lint.sh                       # Linux/macOS
./lint.sh dsp/PhysiX
./lint.sh --fix
```

To build/test just the tool:

```
cmake -S . -B build -G Ninja -DLLVM_DIR=<prefix>/lib/cmake/llvm -DClang_DIR=<prefix>/lib/cmake/clang
cmake --build build
ctest --test-dir build --output-on-failure
```
