if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED RELEASE_TAG)
    message(FATAL_ERROR "RELEASE_TAG is required")
endif()

file(READ "${SOURCE_DIR}/CMakeLists.txt" root_cmake)
string(REGEX MATCH
    "set\\(NEWCONVERT9918_VERSION[ \\t\\r\\n]+\"([^\"]+)\"\\)"
    version_declaration "${root_cmake}")
if(NOT version_declaration)
    message(FATAL_ERROR
        "Could not read NEWCONVERT9918_VERSION from CMakeLists.txt")
endif()
set(source_version "${CMAKE_MATCH_1}")
set(expected_tag "v${source_version}")
if(NOT RELEASE_TAG STREQUAL expected_tag)
    message(FATAL_ERROR
        "Release tag ${RELEASE_TAG} does not match CMake version ${expected_tag}")
endif()
message(STATUS "Release version verified: ${source_version} (${RELEASE_TAG})")
