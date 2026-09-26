foreach(required_variable ROOT GUI_REL CLI_REL SOURCE_IMAGE OUTPUT_DIR VERSION)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

cmake_path(ABSOLUTE_PATH ROOT NORMALIZE OUTPUT_VARIABLE package_root)
set(gui "${package_root}/${GUI_REL}")
set(cli "${package_root}/${CLI_REL}")
file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

execute_process(
    COMMAND "${gui}" --smoke-test
    RESULT_VARIABLE gui_result
    OUTPUT_VARIABLE gui_output
    ERROR_VARIABLE gui_error
    TIMEOUT 30
)
if(NOT gui_result EQUAL 0)
    message(FATAL_ERROR
        "Packaged GUI smoke test failed (${gui_result})\n${gui_output}\n${gui_error}")
endif()

execute_process(
    COMMAND "${cli}" --version
    RESULT_VARIABLE version_result
    OUTPUT_VARIABLE version_output
    ERROR_VARIABLE version_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    TIMEOUT 15
)
if(NOT version_result EQUAL 0 OR
        NOT version_output MATCHES "${VERSION}")
    message(FATAL_ERROR
        "Packaged CLI version check failed (${version_result}): "
        "${version_output}${version_error}")
endif()

execute_process(
    COMMAND "${cli}"
        --input "${SOURCE_IMAGE}"
        --output "${OUTPUT_DIR}"
        --mode bitmap-9918a
        --preset balanced
        --format raw
        --json
    RESULT_VARIABLE conversion_result
    OUTPUT_VARIABLE conversion_output
    ERROR_VARIABLE conversion_error
    TIMEOUT 120
)
if(NOT conversion_result EQUAL 0 OR
        NOT conversion_output MATCHES "\"status\":\"ok\"")
    message(FATAL_ERROR
        "Packaged CLI conversion failed (${conversion_result})\n"
        "${conversion_output}\n${conversion_error}")
endif()

file(GLOB exported_files LIST_DIRECTORIES FALSE "${OUTPUT_DIR}/*")
if(NOT exported_files)
    message(FATAL_ERROR "Packaged CLI produced no export files")
endif()
message(STATUS "Packaged GUI launch and CLI conversion smoke tests passed")
