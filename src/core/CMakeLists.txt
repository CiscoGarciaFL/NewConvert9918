add_library(retrovdp_core STATIC
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
    TargetProfile.cpp
    Validation.cpp
)

target_include_directories(retrovdp_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(retrovdp_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(retrovdp_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(retrovdp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Convert9918's Release quantizers used the fast floating-point model. Limit
# that parity setting to the exhaustive-search implementation files so public
# validation retains strict IEEE handling for NaN and infinity.
set(RETROVDP_HOT_QUANTIZER_SOURCES
    Bitmap9918Converter.cpp
    F18AConverter.cpp
    Multicolor9918Converter.cpp
)
if(MSVC)
    set_property(SOURCE ${RETROVDP_HOT_QUANTIZER_SOURCES} APPEND PROPERTY
        COMPILE_OPTIONS $<$<CONFIG:Release>:/fp:fast>)
else()
    set_property(SOURCE ${RETROVDP_HOT_QUANTIZER_SOURCES} APPEND PROPERTY
        COMPILE_OPTIONS $<$<CONFIG:Release>:-ffast-math>)
endif()
