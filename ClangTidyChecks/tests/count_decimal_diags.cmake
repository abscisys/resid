# Runs synthaxes-clang-tidy (decimal-in-s only) over the fixture and asserts it reports EXACTLY the
# expected number of violations -- which proves it flags the bare literals AND leaves the S-wrapped
# ones, the integer, and the macro-body literal alone. Invoked by tests/CMakeLists.txt.
execute_process(
    COMMAND "${TOOL}" "${FIXTURE}" --quiet --checks=-*,synthaxes-decimal-in-s -- -std=c++17
    OUTPUT_VARIABLE out ERROR_VARIABLE err)

set(all "${out}${err}")
string(REGEX MATCHALL "must be wrapped in the S" hits "${all}")
list(LENGTH hits n)

set(expected 5)   # 0.5 ; 1.0 ; 2.0 ; 1e-5 ; 3e-4  (see fixtures/decimal_in_s.cpp)
if(NOT n EQUAL expected)
    message(FATAL_ERROR "expected ${expected} decimal-in-s violations, got ${n}\n---\n${all}")
endif()
message(STATUS "decimal-in-s: ${n} violations as expected")
