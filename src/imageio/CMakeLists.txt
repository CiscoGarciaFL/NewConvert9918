add_library(retrovdp_imageio STATIC
    ExportWriter.cpp
    ImageLoader.cpp
)

target_include_directories(retrovdp_imageio
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(retrovdp_imageio
    PUBLIC
        retrovdp_core
        retrovdp_formats
        Qt6::Core
        Qt6::Gui
)

target_compile_features(retrovdp_imageio PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(retrovdp_imageio PRIVATE /W4 /permissive-)
else()
    target_compile_options(retrovdp_imageio PRIVATE -Wall -Wextra -Wpedantic)
endif()
