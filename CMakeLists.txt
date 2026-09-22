cmake_minimum_required(VERSION 4.1)

project(SID)

option(SIDPLAY_BUILD_CLANG_TIDY_CHECKS "Build the synthaxes-clang-tidy checks (requires an LLVM/Clang dev package)" OFF)
if (SIDPLAY_BUILD_CLANG_TIDY_CHECKS)
  add_subdirectory(ClangTidyChecks)
endif()

set(SRC_VER 0.16)
set(MAJOR_VER 5)
set(MINOR_VER 0)
set(PATCH_VER 0)

file(GLOB CPP src/*.cpp)
file(GLOB HDR include/*.h)

include_directories(include)
add_library(${PROJECT_NAME} ${CPP} ${HDR})

set_target_properties(${PROJECT_NAME} PROPERTIES
  VERSION ${SRC_VER}
  SOVERSION ${MAJOR_VER}
  CXX_STANDARD 23)

target_compile_definitions(${PROJECT_NAME} PRIVATE VERSION="${MAJOR_VER}.${MINOR_VER}.${PATCH_VER}")

install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  FRAMEWORK DESTINATION "/Library/Frameworks"
  PUBLIC_HEADER DESTINATION include/${PROJECT_NAME})

install(FILES ${HDR} DESTINATION include/${PROJECT_NAME})
