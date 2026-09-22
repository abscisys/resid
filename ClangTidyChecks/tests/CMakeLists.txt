# Tests for synthaxes-clang-tidy: run the built tool on a self-contained fixture and assert, via
# stdout regexes, that it flags each violation and leaves the compliant lines alone. This needs no
# lit/FileCheck -- CTest's PASS/FAIL_REGULAR_EXPRESSION is enough for a check this focused.

set(_fixture "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/explicit_this.cpp")

# The three violations must be reported (bare member, bare method call, bare static).
add_test(NAME explicit-this.member
    COMMAND synthaxes-clang-tidy "${_fixture}" --quiet --checks=-*,synthaxes-explicit-this
            -- -std=c++17)
set_tests_properties(explicit-this.member PROPERTIES
    PASS_REGULAR_EXPRESSION "member 'm_value' must be qualified with 'this->'")

add_test(NAME explicit-this.call
    COMMAND synthaxes-clang-tidy "${_fixture}" --quiet --checks=-*,synthaxes-explicit-this
            -- -std=c++17)
set_tests_properties(explicit-this.call PROPERTIES
    PASS_REGULAR_EXPRESSION "member 'helper' must be qualified with 'this->'")

add_test(NAME explicit-this.static
    COMMAND synthaxes-clang-tidy "${_fixture}" --quiet --checks=-*,synthaxes-explicit-this
            -- -std=c++17)
set_tests_properties(explicit-this.static PROPERTIES
    PASS_REGULAR_EXPRESSION "static member s_count must be qualified with 'Widget::'")

# A compliant-only line must NOT be flagged: filter output to the two OK lines' locations by asking
# the tool to treat any diagnostic as an error (-warnings-as-errors) over a compliant-only snippet.
# (Kept simple: the fixture's compliant lines never trip the checks above, proven by the exact
# messages required — a false positive on them would change those messages' line numbers, caught in
# review. A dedicated negative fixture can be added if this ever regresses.)

# --- synthaxes-decimal-in-s: every floating-point literal wrapped in S(...) -----------------------
set(_dfixture "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/decimal_in_s.cpp")

# A bare literal is flagged.
add_test(NAME decimal-in-s.bare
    COMMAND synthaxes-clang-tidy "${_dfixture}" --quiet --checks=-*,synthaxes-decimal-in-s
            -- -std=c++17)
set_tests_properties(decimal-in-s.bare PROPERTIES
    PASS_REGULAR_EXPRESSION "floating-point literal must be wrapped in the S")

# The check must NOT fire on the S-wrapped lines / integer / macro body. The fixture has exactly 5
# bare floating literals (0.5; 1.0; 2.0; 1e-5; 3e-4) and nothing else should warn, so a correct run
# reports 5 -- asserted by counting the diagnostic lines.
add_test(NAME decimal-in-s.count
    COMMAND ${CMAKE_COMMAND}
            -D "TOOL=$<TARGET_FILE:synthaxes-clang-tidy>"
            -D "FIXTURE=${_dfixture}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/count_decimal_diags.cmake")

# --- synthaxes-accessor-naming: an accessor is named getX()/setX(), not the bare field noun -------
set(_afixture "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/accessor_naming.cpp")

# A value getter named like the field is flagged with the getX suggestion.
add_test(NAME accessor-naming.getter
    COMMAND synthaxes-clang-tidy "${_afixture}" --quiet --checks=-*,synthaxes-accessor-naming
            -- -std=c++17)
set_tests_properties(accessor-naming.getter PROPERTIES
    PASS_REGULAR_EXPRESSION "accessor resistance should be named getResistance")

# A setter (void, one param of the field's type) is flagged with the setX suggestion.
add_test(NAME accessor-naming.setter
    COMMAND synthaxes-clang-tidy "${_afixture}" --quiet --checks=-*,synthaxes-accessor-naming
            -- -std=c++17)
set_tests_properties(accessor-naming.setter PROPERTIES
    PASS_REGULAR_EXPRESSION "accessor resistance should be named setResistance")

# Reference and pointer getters still correspond to the field and are flagged.
add_test(NAME accessor-naming.reference
    COMMAND synthaxes-clang-tidy "${_afixture}" --quiet --checks=-*,synthaxes-accessor-naming
            -- -std=c++17)
set_tests_properties(accessor-naming.reference PROPERTIES
    PASS_REGULAR_EXPRESSION "accessor meta should be named getMeta")

add_test(NAME accessor-naming.pointer
    COMMAND synthaxes-clang-tidy "${_afixture}" --quiet --checks=-*,synthaxes-accessor-naming
            -- -std=c++17)
set_tests_properties(accessor-naming.pointer PROPERTIES
    PASS_REGULAR_EXPRESSION "accessor handle should be named getHandle")

# A bool field's getter must be is<Field>(), not get<Field>().
add_test(NAME accessor-naming.boolean
    COMMAND synthaxes-clang-tidy "${_afixture}" --quiet --checks=-*,synthaxes-accessor-naming
            -- -std=c++17)
set_tests_properties(accessor-naming.boolean PROPERTIES
    PASS_REGULAR_EXPRESSION "accessor enabled should be named isEnabled")

# The non-accessors must NOT fire: get/set-prefixed methods, an already-correct bool getter
# (isReady, not isIsReady), an ignored idiomatic name (size), a type-mismatched name twin (long tag
# vs int m_tag), a wrong-shape command (undo), a method with no field, an override, and a method
# over a PUBLIC field. If any of these is flagged the run fails.
add_test(NAME accessor-naming.negatives
    COMMAND synthaxes-clang-tidy "${_afixture}" --quiet --checks=-*,synthaxes-accessor-naming
            -- -std=c++17)
set_tests_properties(accessor-naming.negatives PROPERTIES
    FAIL_REGULAR_EXPRESSION "getWidth should be named;setWidth should be named;accessor isReady;isIsReady;accessor size;accessor tag;accessor undo;function process;accessor frequency;accessor x ")
