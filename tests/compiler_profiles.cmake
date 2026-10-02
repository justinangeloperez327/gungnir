if(NOT DEFINED GUNGNIRC)
    message(FATAL_ERROR "compiler profile test requires GUNGNIRC")
endif()

set(root "${CMAKE_CURRENT_BINARY_DIR}/compiler-profile-cli")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}")

set(structured "${root}/structured.gnr")
set(compatibility "${root}/compatibility.gnr")

file(WRITE "${structured}" "function int answer() { return 42; }\n")
file(WRITE "${compatibility}" "class LegacyController : Controller { Response index() { return text(\"ok\"); } }\n")

execute_process(
    COMMAND "${GUNGNIRC}" "${structured}" --check
    RESULT_VARIABLE structured_default
)
if(NOT structured_default EQUAL 0)
    message(FATAL_ERROR "default gungnirc profile must accept structured source")
endif()

execute_process(
    COMMAND "${GUNGNIRC}" "${structured}" --strict --check
    RESULT_VARIABLE strict_alias
)
if(NOT strict_alias EQUAL 0)
    message(FATAL_ERROR "--strict must remain a structured-profile compatibility alias")
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
