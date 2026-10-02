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

foreach(file_name IN ITEMS program.validated program.ir modules.cpp)
    file(SHA256 "${GUNGNIR_OUTPUT_DIR}/${file_name}" file_hash)
    file(APPEND
        "${GUNGNIR_OUTPUT_DIR}/manifest.txt"
        "${file_name} ${file_hash}\n"
    )
endforeach()
