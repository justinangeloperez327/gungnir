if(NOT DEFINED GUNGNIRC)
    message(FATAL_ERROR "compiler release contract test requires GUNGNIRC")
endif()
if(NOT DEFINED GUNGNIR_VERSION)
    message(FATAL_ERROR "compiler release contract test requires GUNGNIR_VERSION")
endif()

execute_process(
    COMMAND "${GUNGNIRC}" --version
    RESULT_VARIABLE version_result
    OUTPUT_VARIABLE version_output
    ERROR_VARIABLE version_error
)
if(NOT version_result EQUAL 0)
    message(FATAL_ERROR "gungnirc --version failed: ${version_error}")
endif()

string(REPLACE "\r\n" "\n" version_output "${version_output}")
set(
    expected_version
    "Gungnir compiler ${GUNGNIR_VERSION} (language development)\n"
)
if(NOT version_output STREQUAL expected_version)
    message(
        FATAL_ERROR
        "compiler version contract mismatch:\n"
        "actual: ${version_output}\n"
        "expected: ${expected_version}"
    )
endif()

execute_process(
    COMMAND "${GUNGNIRC}" --print-contract
    RESULT_VARIABLE contract_result
    OUTPUT_VARIABLE contract_output
    ERROR_VARIABLE contract_error
)
if(NOT contract_result EQUAL 0)
    message(
        FATAL_ERROR
        "gungnirc --print-contract failed: ${contract_error}"
    )
endif()

string(REPLACE "\r\n" "\n" contract_output "${contract_output}")
string(
    CONCAT expected_contract
    "package_version=${GUNGNIR_VERSION}\n"
    "language_version=development\n"
    "compiler_contract=development\n"
    "diagnostic_contract=development\n"
    "structured_feature_freeze=false\n"
    "compatibility=experimental\n"
)
if(NOT contract_output STREQUAL expected_contract)
    message(
        FATAL_ERROR
        "compiler contract metadata changed unexpectedly:\n"
        "${contract_output}"
    )
endif()
