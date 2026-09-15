qt_add_executable(test_core_validation
    test_validation.cpp
)

target_link_libraries(test_core_validation
    PRIVATE
        newconvert9918_core
        Qt6::Test
)

add_test(NAME core_validation COMMAND test_core_validation)

