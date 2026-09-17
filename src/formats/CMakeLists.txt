add_library(newconvert9918_formats STATIC
    Export.cpp
)

target_include_directories(newconvert9918_formats
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(newconvert9918_formats
    PUBLIC
        newconvert9918_core
)

target_compile_features(newconvert9918_formats PUBLIC cxx_std_20)
set_target_properties(newconvert9918_formats PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

if(MSVC)
    target_compile_options(newconvert9918_formats PRIVATE /W4 /permissive-)
else()
    target_compile_options(newconvert9918_formats PRIVATE -Wall -Wextra -Wpedantic)
endif()
