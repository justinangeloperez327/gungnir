if(NOT DEFINED GUNGNIRC)
    message(FATAL_ERROR "compiler profile test requires GUNGNIRC")
endif()

set(root "${CMAKE_CURRENT_BINARY_DIR}/compiler-profile-cli")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}")

set(structured "${root}/structured.gnr")
set(compatibility "${root}/compatibility.gnr")
set(invalid_semantic "${root}/invalid-semantic.gnr")

file(WRITE "${structured}" "function int answer() { return 42; }\n")
file(WRITE "${compatibility}" "class LegacyController : Controller { Response index() { return text(\"ok\"); } }\n")
file(WRITE "${invalid_semantic}" "function int missing(bool flag) { if (flag) { return 1; } }\n")

execute_process(
    COMMAND "${GUNGNIRC}" "${structured}" --check
    RESULT_VARIABLE structured_default
    OUTPUT_VARIABLE structured_check_output
)
if(NOT structured_default EQUAL 0)
    message(FATAL_ERROR "default gungnirc profile must accept structured source")
endif()
if(NOT structured_check_output STREQUAL "")
    message(FATAL_ERROR "--check must validate without emitting generated C++")
endif()

execute_process(
    COMMAND "${GUNGNIRC}" "${invalid_semantic}" --check
    RESULT_VARIABLE invalid_semantic_result
    OUTPUT_VARIABLE invalid_semantic_output
    ERROR_VARIABLE invalid_semantic_error
)
if(invalid_semantic_result EQUAL 0)
    message(FATAL_ERROR "--check must reject semantic fallthrough in a non-void callable")
endif()
if(NOT invalid_semantic_output STREQUAL "")
    message(FATAL_ERROR "failed --check must not emit generated C++")
endif()
if(NOT invalid_semantic_error MATCHES "GNR2215")
    message(FATAL_ERROR "--check must report the semantic diagnostic from the structured validator")
endif()

execute_process(
    COMMAND "${GUNGNIRC}" "${structured}" --strict --check
    RESULT_VARIABLE strict_alias
)
if(NOT strict_alias EQUAL 0)
    message(FATAL_ERROR "--strict must remain a structured-profile compatibility alias")
endif()

execute_process(
    COMMAND "${GUNGNIRC}" "${structured}" --dump-cpp-ir --no-line-directives
    RESULT_VARIABLE ir_dump_result
    OUTPUT_VARIABLE ir_dump
)
if(NOT ir_dump_result EQUAL 0)
    message(FATAL_ERROR "--dump-cpp-ir must succeed for structured source")
endif()
if(
    NOT ir_dump MATCHES "cpp-ir structural" OR
    NOT ir_dump MATCHES "function" OR
    NOT ir_dump MATCHES "expression" OR
    NOT ir_dump MATCHES "unit"
)
    message(FATAL_ERROR "--dump-cpp-ir output is missing expected structural IR sections")
endif()

execute_process(
    COMMAND "${GUNGNIRC}" "${compatibility}" --check
    RESULT_VARIABLE legacy_default
    OUTPUT_QUIET
    ERROR_QUIET
)
if(legacy_default EQUAL 0)
    message(FATAL_ERROR "legacy native syntax must not silently use compatibility mode")
endif()

execute_process(
    COMMAND "${GUNGNIRC}" "${compatibility}" --compat --check
    RESULT_VARIABLE explicit_compatibility
)
if(NOT explicit_compatibility EQUAL 0)
    message(FATAL_ERROR "--compat must preserve the legacy source-edit profile")
endif()

execute_process(
    COMMAND "${GUNGNIRC}" "${compatibility}" --compat --project --check
    RESULT_VARIABLE invalid_mix
    OUTPUT_QUIET
    ERROR_QUIET
)
if(invalid_mix EQUAL 0)
    message(FATAL_ERROR "--compat and --project must not be combined")
endif()

execute_process(
    COMMAND "${GUNGNIRC}" "${compatibility}" --compat --dump-cpp-ir
    RESULT_VARIABLE invalid_ir_mix
    OUTPUT_QUIET
    ERROR_QUIET
)
if(invalid_ir_mix EQUAL 0)
    message(FATAL_ERROR "--compat and --dump-cpp-ir must not be combined")
endif()
