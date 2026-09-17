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

add_executable(test_interface_workflow
    test_interface_workflow.cpp
)

target_link_libraries(test_interface_workflow
    PRIVATE
        newconvert9918_app_support
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)

target_compile_definitions(test_interface_workflow
    PRIVATE
        NEWCONVERT9918_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
        NEWCONVERT9918_QML_DIR="${PROJECT_SOURCE_DIR}/app/qml"
)

set_target_properties(test_interface_workflow PROPERTIES AUTOMOC OFF)

add_test(NAME interface_workflow_validation COMMAND test_interface_workflow)
set_tests_properties(interface_workflow_validation PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1.25"
)

add_executable(benchmark_core_conversion
    benchmark_conversion.cpp
)

target_link_libraries(benchmark_core_conversion
    PRIVATE
        newconvert9918_core
)

set_target_properties(benchmark_core_conversion PROPERTIES AUTOMOC OFF)
