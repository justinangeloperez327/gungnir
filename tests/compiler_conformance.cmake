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

foreach(file_name IN ITEMS program.validated program.ir modules.cpp orm.cpp orm.validated orm.ir http.cpp http.validated http.ir context.cpp context.validated context.ir authentication.cpp authentication.validated authentication.ir services.cpp services.validated services.ir routing.cpp routing.validated routing.ir validation.cpp validation.validated validation.ir)
    file(SHA256 "${GUNGNIR_OUTPUT_DIR}/${file_name}" file_hash)
    file(APPEND
        "${GUNGNIR_OUTPUT_DIR}/manifest.txt"
        "${file_name} ${file_hash}\n"
    )
endforeach()
