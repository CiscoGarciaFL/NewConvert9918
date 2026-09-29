add_library(retrovdp_formats STATIC
    Export.cpp
)

target_include_directories(retrovdp_formats
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(retrovdp_formats
    PUBLIC
        retrovdp_core
)

target_compile_features(retrovdp_formats PUBLIC cxx_std_20)
set_target_properties(retrovdp_formats PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

if(MSVC)
    target_compile_options(retrovdp_formats PRIVATE /W4 /permissive-)
else()
    target_compile_options(retrovdp_formats PRIVATE -Wall -Wextra -Wpedantic)
endif()
