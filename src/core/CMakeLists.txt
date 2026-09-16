add_library(newconvert9918_core STATIC
    ColorMath.cpp
    ConversionTypes.cpp
    ImageAdjustments.cpp
    ImageTransform.cpp
    RgbImage.cpp
    TargetData.cpp
    Validation.cpp
)

target_include_directories(newconvert9918_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(newconvert9918_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(newconvert9918_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(newconvert9918_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
