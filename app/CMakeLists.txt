qt_add_executable(NewConvert9918
    main.cpp
)

qt_add_qml_module(NewConvert9918
    URI NewConvert9918
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(NewConvert9918
    PRIVATE
        newconvert9918_core
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickControls2
)

install(TARGETS NewConvert9918
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

