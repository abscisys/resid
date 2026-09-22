# Linux build specifics for synthaxes-clang-tidy.
#
# The clang-tidy static libs + headers come from the LLVM development packages, e.g. from
# apt.llvm.org: llvm-<ver>-dev, libclang-cpp<ver>-dev, clang-tools-<ver>. Point the configure at
# them with -DLLVM_DIR=/usr/lib/llvm-<ver>/lib/cmake/llvm -DClang_DIR=.../lib/cmake/clang.

# Match LLVM's no-RTTI build, or vtable/type_info handling disagrees across the link boundary.
if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(synthaxes-clang-tidy PRIVATE -fno-rtti)
endif()
