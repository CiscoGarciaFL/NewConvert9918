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

add_executable(test_image_input
    test_image_input.cpp
)

target_link_libraries(test_image_input
    PRIVATE
        newconvert9918_imageio
        Qt6::Core
        Qt6::Gui
)

set_target_properties(test_image_input PROPERTIES AUTOMOC OFF)

add_test(NAME image_input_validation COMMAND test_image_input)

add_executable(test_export_formats
    test_export_formats.cpp
)

target_link_libraries(test_export_formats
    PRIVATE
        newconvert9918_formats
)

target_compile_definitions(test_export_formats
    PRIVATE
        NEWCONVERT9918_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
)

set_target_properties(test_export_formats PROPERTIES AUTOMOC OFF)

add_test(NAME export_format_validation COMMAND test_export_formats)

add_executable(test_export_writer
    test_export_writer.cpp
)

target_link_libraries(test_export_writer
    PRIVATE
        newconvert9918_imageio
        Qt6::Core
        Qt6::Gui
)

set_target_properties(test_export_writer PROPERTIES AUTOMOC OFF)

add_test(NAME export_writer_validation COMMAND test_export_writer)

add_executable(benchmark_core_conversion
    benchmark_conversion.cpp
)

target_link_libraries(benchmark_core_conversion
    PRIVATE
        newconvert9918_core
)

set_target_properties(benchmark_core_conversion PROPERTIES AUTOMOC OFF)
