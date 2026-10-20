if(NOT DEFINED GUNGNIRC)
    message(FATAL_ERROR "compiler conformance test requires GUNGNIRC")
endif()
if(NOT DEFINED GUNGNIR_SOURCE_DIR)
    message(FATAL_ERROR "compiler conformance test requires GUNGNIR_SOURCE_DIR")
endif()
if(NOT DEFINED GUNGNIR_OUTPUT_DIR)
    message(FATAL_ERROR "compiler conformance test requires GUNGNIR_OUTPUT_DIR")
endif()

file(REMOVE_RECURSE "${GUNGNIR_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${GUNGNIR_OUTPUT_DIR}")

set(program "${GUNGNIR_SOURCE_DIR}/tests/fixtures/structured/program.gnr")
set(modules "${GUNGNIR_SOURCE_DIR}/tests/fixtures/structured/modules")
set(orm "${GUNGNIR_SOURCE_DIR}/tests/fixtures/structured/orm.gnr")

function(run_gungnirc output)
    execute_process(
        COMMAND "${GUNGNIRC}" ${ARGN} -o "${GUNGNIR_OUTPUT_DIR}/${output}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )
    if(NOT result EQUAL 0)
        message(
            FATAL_ERROR
            "gungnirc conformance command failed for ${output} "
            "(result=${result})\n"
            "executable: ${GUNGNIRC}\n"
            "stdout:\n${stdout}\n"
            "stderr:\n${stderr}"
        )
    endif()

    file(SIZE "${GUNGNIR_OUTPUT_DIR}/${output}" output_size)
    if(output_size EQUAL 0)
        message(FATAL_ERROR "gungnirc produced empty conformance output: ${output}")
    endif()
endfunction()

run_gungnirc(
    "program.cpp"
    "${program}"
    --strict
    --no-line-directives
)

run_gungnirc(
    "program.validated"
    "${program}"
    --dump-validated-ast
    --no-line-directives
)

run_gungnirc(
    "program.ir"
    "${program}"
    --dump-cpp-ir
    --no-line-directives
)

run_gungnirc(
    "modules.cpp"
    "${modules}"
    --project
    --no-line-directives
)

run_gungnirc(
    "program-repeat.cpp"
    "${program}"
    --strict
    --no-line-directives
)

run_gungnirc("orm.cpp" "${orm}" --strict --no-line-directives)
run_gungnirc("orm.validated" "${orm}" --dump-validated-ast --no-line-directives)
run_gungnirc("orm.ir" "${orm}" --dump-cpp-ir --no-line-directives)

set(http "${GUNGNIR_SOURCE_DIR}/tests/fixtures/structured/http.gnr")
run_gungnirc("http.cpp" "${http}" --strict --no-line-directives)
run_gungnirc("http.validated" "${http}" --dump-validated-ast --no-line-directives)
run_gungnirc("http.ir" "${http}" --dump-cpp-ir --no-line-directives)

set(context "${GUNGNIR_SOURCE_DIR}/tests/fixtures/structured/request_context.gnr")
run_gungnirc("context.cpp" "${context}" --strict --no-line-directives)
run_gungnirc("context.validated" "${context}" --dump-validated-ast --no-line-directives)
run_gungnirc("context.ir" "${context}" --dump-cpp-ir --no-line-directives)

set(services "${GUNGNIR_SOURCE_DIR}/tests/fixtures/structured/services.gnr")
run_gungnirc("services.cpp" "${services}" --strict --no-line-directives)
run_gungnirc("services.validated" "${services}" --dump-validated-ast --no-line-directives)
run_gungnirc("services.ir" "${services}" --dump-cpp-ir --no-line-directives)
set(authentication "${GUNGNIR_SOURCE_DIR}/tests/fixtures/structured/authentication.gnr")
run_gungnirc("authentication.cpp" "${authentication}" --strict --no-line-directives)
run_gungnirc("authentication.validated" "${authentication}" --dump-validated-ast --no-line-directives)
run_gungnirc("authentication.ir" "${authentication}" --dump-cpp-ir --no-line-directives)
set(routing "${GUNGNIR_SOURCE_DIR}/tests/fixtures/structured/routing.gnr")
run_gungnirc("routing.cpp" "${routing}" --strict --no-line-directives)
run_gungnirc("routing.validated" "${routing}" --dump-validated-ast --no-line-directives)
run_gungnirc("routing.ir" "${routing}" --dump-cpp-ir --no-line-directives)
set(validation "${GUNGNIR_SOURCE_DIR}/tests/fixtures/structured/validation.gnr")
run_gungnirc("validation.cpp" "${validation}" --strict --no-line-directives)
run_gungnirc("validation.validated" "${validation}" --dump-validated-ast --no-line-directives)
run_gungnirc("validation.ir" "${validation}" --dump-cpp-ir --no-line-directives)
set(background "${GUNGNIR_SOURCE_DIR}/tests/fixtures/structured/background.gnr")
run_gungnirc("background.cpp" "${background}" --strict --no-line-directives)
run_gungnirc("background.validated" "${background}" --dump-validated-ast --no-line-directives)
run_gungnirc("background.ir" "${background}" --dump-cpp-ir --no-line-directives)
set(application "${GUNGNIR_SOURCE_DIR}/tests/fixtures/structured/application.gnr")
run_gungnirc("application.cpp" "${application}" --strict --no-line-directives)
run_gungnirc("application.validated" "${application}" --dump-validated-ast --no-line-directives)
run_gungnirc("application.ir" "${application}" --dump-cpp-ir --no-line-directives)
set(delivery "${GUNGNIR_SOURCE_DIR}/tests/fixtures/structured/delivery.gnr")
run_gungnirc("delivery.cpp" "${delivery}" --strict --no-line-directives)
run_gungnirc("delivery.validated" "${delivery}" --dump-validated-ast --no-line-directives)
run_gungnirc("delivery.ir" "${delivery}" --dump-cpp-ir --no-line-directives)
file(
    SHA256
    "${GUNGNIR_OUTPUT_DIR}/program.cpp"
    first_program_hash
)
file(
    SHA256
    "${GUNGNIR_OUTPUT_DIR}/program-repeat.cpp"
    second_program_hash
)
if(NOT first_program_hash STREQUAL second_program_hash)
    message(FATAL_ERROR "structured compiler output is not deterministic")
endif()

file(REMOVE "${GUNGNIR_OUTPUT_DIR}/program-repeat.cpp")

file(WRITE
    "${GUNGNIR_OUTPUT_DIR}/manifest.txt"
    "program.cpp ${first_program_hash}\n"
)

foreach(file_name IN ITEMS program.validated program.ir modules.cpp orm.cpp orm.validated orm.ir http.cpp http.validated http.ir context.cpp context.validated context.ir authentication.cpp authentication.validated authentication.ir services.cpp services.validated services.ir routing.cpp routing.validated routing.ir validation.cpp validation.validated validation.ir background.cpp background.validated background.ir application.cpp application.validated application.ir delivery.cpp delivery.validated delivery.ir)
    file(SHA256 "${GUNGNIR_OUTPUT_DIR}/${file_name}" file_hash)
    file(APPEND
        "${GUNGNIR_OUTPUT_DIR}/manifest.txt"
        "${file_name} ${file_hash}\n"
    )
endforeach()

# Failed-source diagnostic output is part of the cross-toolchain compiler
# conformance contract, not only successful C++ and IR serialization.
# Use relative input names and an isolated working directory so diagnostic
# file paths do not depend on runner filesystem layout.
file(WRITE "${GUNGNIR_OUTPUT_DIR}/diagnostics.txt" "")
file(WRITE "${GUNGNIR_OUTPUT_DIR}/invalid-lex.gnr" "/* unterminated comment\n")
file(WRITE "${GUNGNIR_OUTPUT_DIR}/invalid-syntax.gnr" "function int broken( {\n")
file(WRITE "${GUNGNIR_OUTPUT_DIR}/invalid-semantic.gnr"
     "function int broken() { return unknown; }\n")

function(snapshot_diagnostic label source mode)
    execute_process(
        COMMAND "${GUNGNIRC}" "${source}" "${mode}"
        WORKING_DIRECTORY "${GUNGNIR_OUTPUT_DIR}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )
    if(NOT status EQUAL 1 OR NOT stdout STREQUAL "")
        message(FATAL_ERROR "${label}: expected a failing, output-free compiler check; exit=${status}; stdout=${stdout}; stderr=${stderr}")
    endif()

    # MSVC's text-mode stderr can use CRLF; normalize line endings only.
    string(REPLACE "\r\n" "\n" stderr "${stderr}")
    string(REGEX MATCH "GNR[0-9][0-9][0-9][0-9]" code "${stderr}")
    if(code STREQUAL "" OR
       NOT stderr MATCHES "${source}:[0-9]+:[0-9]+" OR
       NOT stderr MATCHES "\\^")
        message(FATAL_ERROR "${label}: missing diagnostic code, source position or caret:\n${stderr}")
    endif()

    file(APPEND "${GUNGNIR_OUTPUT_DIR}/diagnostics.txt"
         "=== ${label} ===\n${stderr}\n")
    set("${label}_code" "${code}" PARENT_SCOPE)
endfunction()

snapshot_diagnostic(lex_check "invalid-lex.gnr" --check)
snapshot_diagnostic(lex_format "invalid-lex.gnr" --format)
snapshot_diagnostic(syntax_check "invalid-syntax.gnr" --check)
snapshot_diagnostic(syntax_format "invalid-syntax.gnr" --format)
snapshot_diagnostic(semantic_check "invalid-semantic.gnr" --check)
snapshot_diagnostic(semantic_ir "invalid-semantic.gnr" --verify-ir)

if(NOT lex_check_code STREQUAL lex_format_code OR
   NOT syntax_check_code STREQUAL syntax_format_code OR
   NOT semantic_check_code STREQUAL semantic_ir_code)
    message(FATAL_ERROR "Compiler diagnostic codes differ across equivalent CLI modes")
endif()
# The invalid return also produces a return-type mismatch (GNR2214),
# which may precede the underlying unknown-name diagnostic (GNR2202).
# Require the expected root-cause code anywhere in the full diagnostic output.
# The first-code parity above still detects mode-specific ordering differences.
# Verify the full rendered error includes the unknown-name diagnostic as well.
execute_process(
    COMMAND "${GUNGNIRC}" "invalid-semantic.gnr" --check
    WORKING_DIRECTORY "${GUNGNIR_OUTPUT_DIR}"
    RESULT_VARIABLE semantic_code_result
    OUTPUT_VARIABLE semantic_code_output
    ERROR_VARIABLE semantic_code_error
)
if(NOT semantic_code_result EQUAL 1 OR
   NOT semantic_code_output STREQUAL "" OR
   NOT semantic_code_error MATCHES "GNR2202")
    message(FATAL_ERROR "Expected unknown-name diagnostic GNR2202 in semantic failure output")
endif()

file(SHA256 "${GUNGNIR_OUTPUT_DIR}/diagnostics.txt" diagnostics_hash)
file(APPEND "${GUNGNIR_OUTPUT_DIR}/manifest.txt"
     "diagnostics.txt ${diagnostics_hash}\n")
