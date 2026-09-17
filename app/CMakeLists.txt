add_library(newconvert9918_app_support STATIC
    ImageInputController.cpp
    ImageInputController.hpp
)

target_include_directories(newconvert9918_app_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(newconvert9918_app_support
    PUBLIC
        newconvert9918_core
        newconvert9918_imageio
        Qt6::Core
        Qt6::Gui
)

target_compile_features(newconvert9918_app_support PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(newconvert9918_app_support PRIVATE /W4 /permissive-)
else()
    target_compile_options(newconvert9918_app_support PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(NewConvert9918
    main.cpp
)

set_target_properties(NewConvert9918 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

qt_add_qml_module(NewConvert9918
    URI NewConvert9918
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/PreviewPane.qml
        qml/SettingsPanel.qml
)

target_link_libraries(NewConvert9918
    PRIVATE
        newconvert9918_app_support
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickControls2
)

install(TARGETS NewConvert9918
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
