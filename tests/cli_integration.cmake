if(
    NOT DEFINED GUNGNIR_BUILD_DIR OR
    NOT DEFINED GUNGNIR_CLI
)
    message(
        FATAL_ERROR
        "CLI integration test requires build directory and CLI path"
    )
endif()

set(stage "${GUNGNIR_BUILD_DIR}/cli-stage")
set(project "${GUNGNIR_BUILD_DIR}/cli-sample")

file(REMOVE_RECURSE "${stage}" "${project}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${GUNGNIR_BUILD_DIR}" --prefix "${stage}"
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
    COMMAND "${GUNGNIR_CLI}" make:model User
    WORKING_DIRECTORY "${project}"
    RESULT_VARIABLE model_result
)
if(NOT model_result EQUAL 0)
    message(FATAL_ERROR "gungnir make:model failed")
endif()

execute_process(
    COMMAND "${GUNGNIR_CLI}" make:migration create_users_table
    WORKING_DIRECTORY "${project}"
    RESULT_VARIABLE migration_result
)
if(NOT migration_result EQUAL 0)
    message(FATAL_ERROR "gungnir make:migration failed")
endif()

file(APPEND "${project}/.env" "\nDB_CONNECTION=postgresql\n")

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "GUNGNIR_CMAKE_PREFIX=${stage}"
        "${GUNGNIR_CLI}" build
    WORKING_DIRECTORY "${project}"
    RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Generated Gungnir application failed to build")
endif()

if(WIN32)
    set(app "${project}/.gungnir/build/Debug/app.exe")
    if(NOT EXISTS "${app}")
        set(app "${project}/.gungnir/build/app.exe")
    endif()
else()
    set(app "${project}/.gungnir/build/app")
endif()

if(NOT EXISTS "${app}")
    message(FATAL_ERROR "Generated Gungnir application executable was not created")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "GUNGNIR_CMAKE_PREFIX=${stage}"
        "${GUNGNIR_CLI}" migrate:plan
    WORKING_DIRECTORY "${project}"
    RESULT_VARIABLE plan_result
)
if(NOT plan_result EQUAL 0)
    message(FATAL_ERROR "gungnir migrate:plan failed")
endif()
