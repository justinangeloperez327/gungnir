if(
    NOT DEFINED GUNGNIR_BUILD_DIR OR
    NOT DEFINED GUNGNIR_CLI
)
    message(
        FATAL_ERROR
        "CLI integration test requires build directory and CLI path"
    )
endif()

set(stage "${GUNGNIR_BUILD_DIR}/cli stage")
set(project "${GUNGNIR_BUILD_DIR}/cli sample")

file(REMOVE_RECURSE "${stage}" "${project}")

set(
    install_command
    "${CMAKE_COMMAND}"
    --install
    "${GUNGNIR_BUILD_DIR}"
    --prefix
    "${stage}"
)

if(
    DEFINED GUNGNIR_BUILD_CONFIG AND
    NOT GUNGNIR_BUILD_CONFIG STREQUAL ""
)
    list(
        APPEND
        install_command
        --config
        "${GUNGNIR_BUILD_CONFIG}"
    )
endif()

execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "Gungnir staged install failed")
endif()

execute_process(
    COMMAND "${GUNGNIR_CLI}" new SampleApp "${project}"
    RESULT_VARIABLE new_result
)
if(NOT new_result EQUAL 0)
    message(FATAL_ERROR "gungnir new failed")
endif()

execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${GUNGNIR_SOURCE_DIR}/tests/generated_project.py"
        "${GUNGNIR_CLI}" "${project}" "${stage}" "${GUNGNIR_WITH_SQLITE}"
    RESULT_VARIABLE integration_result
)
if(NOT integration_result EQUAL 0)
    message(FATAL_ERROR "Generated application integration failed")
endif()
