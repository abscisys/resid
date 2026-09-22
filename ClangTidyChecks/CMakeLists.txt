cmake_minimum_required(VERSION 4.1.0)
project(SynthaxesClangTidy LANGUAGES CXX)

# Builds `synthaxes-clang-tidy`: a clang-tidy executable made from the INSTALLED clang-tidy static
# libraries plus our own check(s). No LLVM source build, no loadable plugin (which does not work on
# Windows). find_package resolves against an LLVM *development* distribution -- point it at one with
#   -DLLVM_DIR=<prefix>/lib/cmake/llvm  -DClang_DIR=<prefix>/lib/cmake/clang
#
# The C++ here is 100% platform-agnostic (pure Clang AST analysis, no OS calls). The ONLY platform
# variance is in the build itself -- DIA-SDK path, RTTI flag, toolchain quirks -- and it lives in
# cmake/<System>.cmake, nowhere else.

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
message(STATUS "synthaxes-clang-tidy: LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")

add_executable(synthaxes-clang-tidy
    src/main.cpp
    src/SynthaxesTidyModule.cpp
    src/ThisUsageCheck.cpp
    src/DecimalInSCheck.cpp
    src/AccessorNamingCheck.cpp)

# Match how the LLVM libraries were built (C++17) for ABI safety across the link boundary.
target_compile_features(synthaxes-clang-tidy PRIVATE cxx_std_17)

target_include_directories(synthaxes-clang-tidy PRIVATE include)
target_include_directories(synthaxes-clang-tidy SYSTEM PRIVATE
    ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_definitions(synthaxes-clang-tidy PRIVATE ${LLVM_DEFINITIONS})

# Apply the one platform module that has anything to say (the rest of the build is identical
# everywhere). CMAKE_SYSTEM_NAME is Windows / Linux / Darwin.
set(_synthaxes_platform "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${CMAKE_SYSTEM_NAME}.cmake")
if(EXISTS "${_synthaxes_platform}")
    include("${_synthaxes_platform}")
else()
    message(WARNING "synthaxes-clang-tidy: no platform module cmake/${CMAKE_SYSTEM_NAME}.cmake")
endif()

# The imported clang-tidy targets carry their transitive clang/LLVM dependencies, so this short list
# pulls in everything. clangTidyMain is the entry point; the module libs are the checks our
# .clang-tidy uses (their registration is forced from main.cpp).
target_link_libraries(synthaxes-clang-tidy PRIVATE
    clangTidyMain
    clangTidy
    clangTidyUtils
    clangTidyReadabilityModule
    clangTidyBugproneModule
    clangTidyPerformanceModule
    clangTidyMiscModule
    clangTidyModernizeModule)

# The clangTidy* archives call into LLVM, but naming them does not put libLLVM on
# the link line: on a distribution that ships LLVM as one shared object -- Debian
# and Ubuntu do -- every one of those calls is left unresolved with "DSO missing
# from command line". The `LLVM` imported target is not enough here; it resolves
# but contributes no library, so the dylib is located and linked by path.
find_library(SYNTHAXES_LLVM_DYLIB
    NAMES LLVM LLVM-${LLVM_VERSION_MAJOR}
    HINTS "${LLVM_LIBRARY_DIR}" "${LLVM_LIBRARY_DIRS}" "${LLVM_DIR}/../.."
    NO_DEFAULT_PATH)

if(SYNTHAXES_LLVM_DYLIB)
    message(STATUS "synthaxes-clang-tidy: linking ${SYNTHAXES_LLVM_DYLIB}")
    target_link_libraries(synthaxes-clang-tidy PRIVATE "${SYNTHAXES_LLVM_DYLIB}")
else()
    llvm_map_components_to_libnames(_llvm_libs support core frontendopenmp)
    target_link_libraries(synthaxes-clang-tidy PRIVATE ${_llvm_libs})
endif()

# The built tool lands in bin/ (gitignored); the launch scripts run it from there.
set_target_properties(synthaxes-clang-tidy PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

enable_testing()
add_subdirectory(tests)
