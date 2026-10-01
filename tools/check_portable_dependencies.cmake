if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(PORTABLE_ROOTS
    "${SOURCE_ROOT}/include/retrovdp/core"
    "${SOURCE_ROOT}/include/retrovdp/formats"
    "${SOURCE_ROOT}/src/core"
    "${SOURCE_ROOT}/src/formats"
)

set(PORTABLE_FILES)
foreach(ROOT IN LISTS PORTABLE_ROOTS)
    file(GLOB_RECURSE ROOT_FILES
        "${ROOT}/*.cpp"
        "${ROOT}/*.hpp"
    )
    list(APPEND PORTABLE_FILES ${ROOT_FILES})
endforeach()

foreach(FILE IN LISTS PORTABLE_FILES)
    file(READ "${FILE}" CONTENTS)
    if(CONTENTS MATCHES "#[ \t]*include[ \t]*[<\"](Q[A-Za-z0-9_/]+|Qt[A-Za-z0-9_/]*)[>\"]")
        file(RELATIVE_PATH RELATIVE_FILE "${SOURCE_ROOT}" "${FILE}")
        message(FATAL_ERROR
            "Portable core dependency violation: ${RELATIVE_FILE} includes a Qt header")
    endif()
endforeach()

message(STATUS "Portable core dependency boundary is clean")
