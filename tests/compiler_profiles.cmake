if(NOT DEFINED GUNGNIRC)
    message(FATAL_ERROR "compiler profile test requires GUNGNIRC")
endif()

set(root "${CMAKE_CURRENT_BINARY_DIR}/compiler-profile-cli")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}")

set(structured "${root}/structured.gnr")
set(compatibility "${root}/compatibility.gnr")
set(invalid_semantic "${root}/invalid-semantic.gnr")
set(invalid_syntax "${root}/invalid-syntax.gnr")

file(WRITE "${structured}" "function int answer() { return 42; }\n")
file(WRITE "${compatibility}" "class LegacyController : Controller { Response index() { return text(\"ok\"); } }\n")
file(WRITE "${invalid_semantic}" "function int missing(bool flag) { if (flag) { return 1; } }\n")
file(WRITE "${invalid_syntax}" "function int broken( {\n")

foreach(flow_case IN ITEMS branch loop lambda)
    set(flow_source "function int broken(int? input, bool reset) { let value = input; if (value == null) { return 0; } if (reset) { value = null; } return value; }")
    if(flow_case STREQUAL "loop")
        set(flow_source "function int broken(int? input, bool reset) { let value = input; if (value == null) { return 0; } while (reset) { value = null; break; } return value; }")
    elseif(flow_case STREQUAL "lambda")
        set(flow_source "function int broken(int? input) { let value = input; const read = () => { if (value == null) { return 0; } return value; }; return value; }")
    endif()
    set(flow_input "${root}/invalid-null-flow-${flow_case}.gnr")
    file(WRITE "${flow_input}" "${flow_source}\n")
    foreach(mode IN ITEMS --check --verify-ir)
        execute_process(
            COMMAND "${GUNGNIRC}" "${flow_input}" "${mode}"
            RESULT_VARIABLE flow_result
            OUTPUT_VARIABLE flow_output
            ERROR_VARIABLE flow_error
        )
        if(NOT flow_result EQUAL 1 OR NOT flow_output STREQUAL "" OR
           NOT flow_error MATCHES "GNR2214")
            message(FATAL_ERROR "${mode} must reject stale ${flow_case} null facts: ${flow_error}")
        endif()
    endforeach()
endforeach()

# /dev/full opens successfully but rejects writes, including buffered flushes.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND EXISTS "/dev/full")
    foreach(mode IN ITEMS compile format ast ir compat)
        set(arguments)
        set(source "${structured}")
        if(mode STREQUAL "format")
            set(arguments --format)
        elseif(mode STREQUAL "ast")
            set(arguments --dump-validated-ast)
        elseif(mode STREQUAL "ir")
            set(arguments --dump-cpp-ir)
        elseif(mode STREQUAL "compat")
            set(arguments --compat)
            set(source "${compatibility}")
        endif()
        foreach(destination IN ITEMS file stdout)
            if(destination STREQUAL "file")
                execute_process(
                    COMMAND "${GUNGNIRC}" "${source}" ${arguments} -o /dev/full
                    RESULT_VARIABLE write_result
                    OUTPUT_VARIABLE write_output
                    ERROR_VARIABLE write_error
                )
            else()
                execute_process(
                    COMMAND "${GUNGNIRC}" "${source}" ${arguments}
                    RESULT_VARIABLE write_result
                    OUTPUT_FILE /dev/full
                    ERROR_VARIABLE write_error
                )
            endif()
            if(NOT write_result EQUAL 1 OR NOT write_error MATCHES "Unable to write")
                message(FATAL_ERROR "${mode} must report ${destination} write/flush failures: ${write_error}")
            endif()
        endforeach()
    endforeach()
endif()

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

# --verify-ir performs lower/verify after semantic validation without emission.
execute_process(
    COMMAND "${GUNGNIRC}" "${structured}" --verify-ir
    RESULT_VARIABLE verify_ir_result
    OUTPUT_VARIABLE verify_ir_output
    ERROR_VARIABLE verify_ir_error
)
if(NOT verify_ir_result EQUAL 0 OR NOT verify_ir_output STREQUAL "")
    message(FATAL_ERROR "--verify-ir must verify structural IR without emitting C++: ${verify_ir_error}")
endif()

set(verified_output "${root}/should-not-exist.cpp")
execute_process(
    COMMAND "${GUNGNIRC}" "${structured}" --verify-ir -o "${verified_output}"
    RESULT_VARIABLE verify_ir_output_rejected
    OUTPUT_QUIET
    ERROR_QUIET
)
if(verify_ir_output_rejected EQUAL 0 OR EXISTS "${verified_output}")
    message(FATAL_ERROR "--verify-ir must reject output-file selection")
endif()

file(MAKE_DIRECTORY "${root}/sample/storage")
file(WRITE "${root}/sample/answer.gnr" "function int answer() { return 42; }\n")
file(WRITE "${root}/sample/storage/invalid.gnr" "function int broken( {\n")
execute_process(
    COMMAND "${GUNGNIRC}" "${root}/sample" --project --verify-ir
    RESULT_VARIABLE verify_project_result
    OUTPUT_VARIABLE verify_project_output
    ERROR_VARIABLE verify_project_error
)
if(NOT verify_project_result EQUAL 0 OR NOT verify_project_output STREQUAL "")
    message(FATAL_ERROR "--project --verify-ir must succeed without C++ emission: ${verify_project_error}")
endif()

execute_process(
    COMMAND "${GUNGNIRC}" "${invalid_semantic}" --verify-ir
    RESULT_VARIABLE verify_invalid_result
    OUTPUT_VARIABLE verify_invalid_output
    ERROR_VARIABLE verify_invalid_error
)
if(verify_invalid_result EQUAL 0 OR NOT verify_invalid_output STREQUAL "" OR
   NOT verify_invalid_error MATCHES "GNR2215")
    message(FATAL_ERROR "--verify-ir must preserve semantic diagnostic failures")
endif()

foreach(conflict IN ITEMS --check --format --compat --dump-cpp-ir --dump-validated-ast)
    execute_process(
        COMMAND "${GUNGNIRC}" "${structured}" --verify-ir "${conflict}"
        RESULT_VARIABLE verify_conflict_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(verify_conflict_result EQUAL 0)
        message(FATAL_ERROR "--verify-ir must reject ${conflict}")
    endif()
endforeach()

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
if(NOT invalid_semantic_error MATCHES "\\^")
    message(FATAL_ERROR "--check diagnostics must render source context and a caret")
endif()

execute_process(
    COMMAND "${GUNGNIRC}" "${structured}" --strict --check
    RESULT_VARIABLE strict_alias
)
if(NOT strict_alias EQUAL 0)
    message(FATAL_ERROR "--strict must remain a structured-profile compatibility alias")
endif()

# Explicit compiler modes are mutually exclusive regardless of flag order.
foreach(flags IN ITEMS "--compat;--strict" "--strict;--compat")
    execute_process(
        COMMAND "${GUNGNIRC}" "${structured}" ${flags} --check
        RESULT_VARIABLE conflicting_modes_result
        OUTPUT_VARIABLE conflicting_modes_output
        ERROR_VARIABLE conflicting_modes_error
    )
    if(NOT conflicting_modes_result EQUAL 2 OR
       NOT conflicting_modes_output STREQUAL "" OR
       NOT conflicting_modes_error MATCHES "--compat and --strict are mutually exclusive")
        message(FATAL_ERROR "Conflicting compiler profiles must fail with usage error in both orders: ${flags}")
    endif()
endforeach()

# Formatting errors must retain source location, diagnostic code, and caret.
execute_process(
    COMMAND "${GUNGNIRC}" "${invalid_syntax}" --check
    RESULT_VARIABLE syntax_check_result
    OUTPUT_VARIABLE syntax_check_output
    ERROR_VARIABLE syntax_check_error
)
execute_process(
    COMMAND "${GUNGNIRC}" "${invalid_syntax}" --format
    RESULT_VARIABLE syntax_format_result
    OUTPUT_VARIABLE syntax_format_output
    ERROR_VARIABLE syntax_format_error
)
if(NOT syntax_check_result EQUAL 1 OR NOT syntax_format_result EQUAL 1 OR
   NOT syntax_check_output STREQUAL "" OR NOT syntax_format_output STREQUAL "" OR
   NOT syntax_check_error MATCHES "GNR[0-9][0-9][0-9][0-9]" OR
   NOT syntax_format_error MATCHES "GNR[0-9][0-9][0-9][0-9]" OR
   NOT syntax_format_error MATCHES "invalid-syntax.gnr" OR
   NOT syntax_format_error MATCHES "\\^")
    message(FATAL_ERROR "--format syntax errors must carry normal structured diagnostics: ${syntax_format_error}")
endif()
string(REGEX MATCH "GNR[0-9][0-9][0-9][0-9]" checked_syntax_code "${syntax_check_error}")
string(REGEX MATCH "GNR[0-9][0-9][0-9][0-9]" formatted_syntax_code "${syntax_format_error}")
if(NOT checked_syntax_code STREQUAL formatted_syntax_code)
    message(FATAL_ERROR "--format must preserve the same primary syntax diagnostic code as --check")
endif()

set(format_output "${root}/invalid-format.cpp")
execute_process(
    COMMAND "${GUNGNIRC}" "${invalid_syntax}" --format -o "${format_output}"
    RESULT_VARIABLE syntax_format_file_result
    OUTPUT_QUIET
    ERROR_QUIET
)
if(NOT syntax_format_file_result EQUAL 1 OR EXISTS "${format_output}")
    message(FATAL_ERROR "Invalid formatted source must never write an output file")
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
