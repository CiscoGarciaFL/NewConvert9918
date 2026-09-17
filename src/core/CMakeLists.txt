add_library(newconvert9918_core STATIC
    Bitmap9918Converter.cpp
    ColorMath.cpp
    ConversionJobController.cpp
    ConversionPerformance.cpp
    ConversionTypes.cpp
    Dithering.cpp
    F18AConverter.cpp
    ImageDecoding.cpp
    ImageAdjustments.cpp
    ImageTransform.cpp
    Multicolor9918Converter.cpp
    PaletteSelection.cpp
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
