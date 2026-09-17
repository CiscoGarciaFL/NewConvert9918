add_library(newconvert9918_imageio STATIC
    ExportWriter.cpp
    ImageLoader.cpp
)

target_include_directories(newconvert9918_imageio
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(newconvert9918_imageio
    PUBLIC
        newconvert9918_core
        newconvert9918_formats
        Qt6::Core
        Qt6::Gui
)

target_compile_features(newconvert9918_imageio PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(newconvert9918_imageio PRIVATE /W4 /permissive-)
else()
    target_compile_options(newconvert9918_imageio PRIVATE -Wall -Wextra -Wpedantic)
endif()
