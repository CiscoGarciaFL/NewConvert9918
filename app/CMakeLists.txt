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
        qml/PreviewWorkspace.qml
        qml/SettingsPanel.qml
    RESOURCES
        assets/icons/NewConvert9918-256.png
)

if(WIN32)
    set(NEWCONVERT9918_WINDOWS_ICON
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/NewConvert9918.ico"
    )
    configure_file(
        assets/windows/NewConvert9918.rc.in
        "${CMAKE_CURRENT_BINARY_DIR}/NewConvert9918.rc"
        @ONLY
    )
    target_sources(NewConvert9918 PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/NewConvert9918.rc"
    )
endif()

if(APPLE)
    set(NEWCONVERT9918_MACOS_ICON
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/NewConvert9918.icns"
    )
    set_source_files_properties("${NEWCONVERT9918_MACOS_ICON}" PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(NewConvert9918 PRIVATE "${NEWCONVERT9918_MACOS_ICON}")
    set_target_properties(NewConvert9918 PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE NewConvert9918.icns
    )
endif()

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

if(UNIX AND NOT APPLE)
    foreach(icon_size 16 24 32 48 64 128 256 512 1024)
        install(
            FILES "assets/icons/NewConvert9918-${icon_size}.png"
            DESTINATION "share/icons/hicolor/${icon_size}x${icon_size}/apps"
            RENAME NewConvert9918.png
        )
    endforeach()
    install(
        FILES assets/NewConvert9918.svg
        DESTINATION share/icons/hicolor/scalable/apps
    )
endif()
