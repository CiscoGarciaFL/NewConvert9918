add_executable(test_core_validation
    test_validation.cpp
)

target_link_libraries(test_core_validation
    PRIVATE
        newconvert9918_core
        Qt6::Gui
)

target_compile_definitions(test_core_validation
    PRIVATE
        NEWCONVERT9918_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
)

# The test uses a small standalone runner and requires no Qt meta-object code.
set_target_properties(test_core_validation PROPERTIES AUTOMOC OFF)

add_test(NAME core_validation COMMAND test_core_validation)
