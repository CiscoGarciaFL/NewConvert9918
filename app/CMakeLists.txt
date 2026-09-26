add_library(newconvert9918_app_support STATIC
    AppPreferencesController.cpp
    AppPreferencesController.hpp
    ConversionPipeline.cpp
    ConversionPipeline.hpp
    EditorProjectController.cpp
    EditorProjectControllerSprite.cpp
    EditorProjectController.hpp
    ImageInputController.cpp
    ImageInputController.hpp
)

# Keep Qt's autogen subprocess count conservative on Windows developer hosts.
set_property(TARGET newconvert9918_app_support PROPERTY AUTOGEN_PARALLEL 1)

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
set_property(TARGET NewConvert9918 PROPERTY AUTOGEN_PARALLEL 1)

set_target_properties(NewConvert9918 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_compile_definitions(NewConvert9918
    PRIVATE NEWCONVERT9918_VERSION="${NEWCONVERT9918_VERSION}"
)

qt_add_qml_module(NewConvert9918
    URI NewConvert9918
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/PreferencesDialog.qml
        qml/PreviewPane.qml
        qml/PreviewWorkspace.qml
        qml/SettingsPanel.qml
        qml/EditorWorkspacePlaceholder.qml
        qml/EditorSidePanelPlaceholder.qml
        qml/SidePanelHost.qml
        qml/OutputProfileControls.qml
        qml/CharacterSetView.qml
        qml/CharacterTilingView.qml
        qml/PatternEditor.qml
        qml/SpritePatternEditor.qml
        qml/SpritePlacementView.qml
        qml/SpriteSetView.qml
        qml/ThemedFrame.qml
        qml/ThemedGroupBox.qml
    RESOURCES
        assets/icons/NewConvert9918-256.png
        assets/icons/NewConvert9918-watermark-512.png
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
        MACOSX_BUNDLE_GUI_IDENTIFIER "${NEWCONVERT9918_MACOS_BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_NAME "New Convert 9918"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

target_link_libraries(NewConvert9918
    PRIVATE
        newconvert9918_app_support
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
)

install(TARGETS NewConvert9918
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

set(NEWCONVERT9918_DEPLOY_TOOL_OPTIONS)
if(APPLE)
    # Preview builds use an ad-hoc signature. Stable releases replace this
    # with the protected Developer ID identity in release automation.
    list(APPEND NEWCONVERT9918_DEPLOY_TOOL_OPTIONS "-codesign=-")
endif()

qt_generate_deploy_qml_app_script(
    TARGET NewConvert9918
    OUTPUT_SCRIPT NEWCONVERT9918_DEPLOY_SCRIPT
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
    EXCLUDE_PLUGIN_TYPES qmltooling
    DEPLOY_TOOL_OPTIONS ${NEWCONVERT9918_DEPLOY_TOOL_OPTIONS}
)
install(SCRIPT ${NEWCONVERT9918_DEPLOY_SCRIPT} COMPONENT Runtime)

qt_add_executable(NewConvert9918Cli
    cli_main.cpp
)
set_property(TARGET NewConvert9918Cli PROPERTY AUTOGEN_PARALLEL 1)

set_target_properties(NewConvert9918Cli PROPERTIES
    OUTPUT_NAME newconvert9918-cli
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE FALSE
    MACOSX_BUNDLE FALSE
)

if(APPLE)
    set_target_properties(NewConvert9918Cli PROPERTIES
        INSTALL_RPATH
            "@loader_path/../NewConvert9918.app/Contents/Frameworks"
    )
elseif(UNIX)
    set_target_properties(NewConvert9918 NewConvert9918Cli PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib"
    )
endif()

target_compile_definitions(NewConvert9918Cli
    PRIVATE NEWCONVERT9918_VERSION="${NEWCONVERT9918_VERSION}"
)

target_link_libraries(NewConvert9918Cli
    PRIVATE
        newconvert9918_app_support
        Qt6::Core
        Qt6::Gui
)

install(TARGETS NewConvert9918Cli
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

if(UNIX AND NOT APPLE)
    foreach(icon_size 16 24 32 48 64 128 256 512 1024)
        install(
            FILES "assets/icons/NewConvert9918-${icon_size}.png"
            DESTINATION "share/icons/hicolor/${icon_size}x${icon_size}/apps"
            RENAME "${NEWCONVERT9918_LINUX_APP_ID}.png"
            COMPONENT Runtime
        )
    endforeach()
    install(
        FILES assets/NewConvert9918.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME "${NEWCONVERT9918_LINUX_APP_ID}.svg"
        COMPONENT Runtime
    )
    configure_file(
        assets/linux/io.github.ciscogarciafl.NewConvert9918.desktop.in
        "${CMAKE_CURRENT_BINARY_DIR}/${NEWCONVERT9918_LINUX_APP_ID}.desktop"
        @ONLY
    )
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/${NEWCONVERT9918_LINUX_APP_ID}.desktop"
        DESTINATION share/applications
        COMPONENT Runtime
    )
endif()
