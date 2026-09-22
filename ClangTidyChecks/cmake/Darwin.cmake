# macOS build specifics for synthaxes-clang-tidy.
#
# AppleClang ships neither clang-tidy nor its dev libraries; install them from Homebrew
# (`brew install llvm`) and point the configure at that prefix:
#   -DLLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm -DClang_DIR=$(brew --prefix llvm)/lib/cmake/clang

# Match LLVM's no-RTTI build, or vtable/type_info handling disagrees across the link boundary.
if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(synthaxes-clang-tidy PRIVATE -fno-rtti)
endif()
