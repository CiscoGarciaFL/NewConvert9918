cmake_minimum_required(VERSION 3.24)

# This is the single release-version source. CMake's project() command accepts
# only the numeric core, so keep the complete semantic version here and derive
# PROJECT_VERSION from it.
set(NEWCONVERT9918_VERSION "0.1.0-beta.2")
if(NOT NEWCONVERT9918_VERSION MATCHES
        "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[0-9A-Za-z.-]+)?$")
    message(FATAL_ERROR
        "NEWCONVERT9918_VERSION is not a valid semantic version: "
        "${NEWCONVERT9918_VERSION}")
endif()
set(NEWCONVERT9918_VERSION_CORE
    "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")

if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    # Qt 6.10 supports macOS 13 and newer. Keep this explicit so both native
    # architecture packages have the same compatibility floor.
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING
        "Minimum supported macOS release")
endif()

project(NewConvert9918
    VERSION ${NEWCONVERT9918_VERSION_CORE}
    DESCRIPTION "Cross-platform image converter for TMS9918A and F18A graphics"
    LANGUAGES CXX
)

set(NEWCONVERT9918_ORGANIZATION "CiscoGarciaFL")
set(NEWCONVERT9918_SETTINGS_ID "NewConvert9918")
set(NEWCONVERT9918_MACOS_BUNDLE_ID "io.github.ciscogarciafl.NewConvert9918")
set(NEWCONVERT9918_LINUX_APP_ID "io.github.ciscogarciafl.NewConvert9918")
# Frozen before the first beta. This identifies the Windows installer family;
# never replace it for later beta, RC, or stable packages.
set(NEWCONVERT9918_WINDOWS_INSTALLER_ID
    "{9D53324F-AE3A-42FC-96C6-0B2256798410}")

option(NEWCONVERT9918_RELEASE_BUILD
    "Enable strict release packaging inputs and metadata" OFF)
if(NEWCONVERT9918_RELEASE_BUILD)
    set(NEWCONVERT9918_LICENSE_DIR "${CMAKE_BINARY_DIR}/release-licenses")
    file(MAKE_DIRECTORY "${NEWCONVERT9918_LICENSE_DIR}")
    file(DOWNLOAD
        "https://www.gnu.org/licenses/lgpl-3.0.txt"
        "${NEWCONVERT9918_LICENSE_DIR}/LGPL-3.0.txt"
        EXPECTED_HASH
            "SHA256=e3a994d82e644b03a792a930f574002658412f62407f5fee083f2555c5f23118"
        TLS_VERIFY ON
        TIMEOUT 60
    )
    file(DOWNLOAD
        "https://www.gnu.org/licenses/gpl-3.0.txt"
        "${NEWCONVERT9918_LICENSE_DIR}/GPL-3.0.txt"
        EXPECTED_HASH
            "SHA256=3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986"
        TLS_VERIFY ON
        TIMEOUT 60
    )
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The legacy Release build used whole-program optimization. Enable the
# equivalent portable IPO/LTO mode when the active compiler supports it so
# cross-translation-unit helpers can be inlined in production builds.
include(CheckIPOSupported)
check_ipo_supported(RESULT NEWCONVERT9918_IPO_SUPPORTED)
if(NEWCONVERT9918_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Widgets)
qt_standard_project_setup(REQUIRES 6.8)

add_subdirectory(src/core)
add_subdirectory(src/formats)
add_subdirectory(src/imageio)
add_subdirectory(app)

install(FILES
    LICENSE
    NOTICE.md
    docs/THIRD_PARTY_NOTICES.md
    DESTINATION .
    COMPONENT Runtime
)
if(NEWCONVERT9918_RELEASE_BUILD)
    install(DIRECTORY "${NEWCONVERT9918_LICENSE_DIR}/"
        DESTINATION LICENSES
        COMPONENT Runtime
    )
endif()

set(CPACK_PACKAGE_NAME "NewConvert9918")
set(CPACK_PACKAGE_VENDOR "CiscoGarciaFL")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL
    "https://github.com/CiscoGarciaFL/NewConvert9918")
set(CPACK_PACKAGE_VERSION "${NEWCONVERT9918_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "NewConvert9918")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_MONOLITHIC_INSTALL ON)

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "New Convert 9918")
    set(CPACK_NSIS_PACKAGE_NAME "New Convert 9918")
    set(CPACK_NSIS_INSTALL_ROOT "$LOCALAPPDATA")
    set(CPACK_NSIS_MUI_ICON
        "${CMAKE_CURRENT_SOURCE_DIR}/app/assets/icons/NewConvert9918.ico")
    set(CPACK_NSIS_MUI_UNIICON
        "${CMAKE_CURRENT_SOURCE_DIR}/app/assets/icons/NewConvert9918.ico")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY
        "NewConvert9918-${NEWCONVERT9918_WINDOWS_INSTALLER_ID}")
    set(CPACK_PACKAGE_EXECUTABLES "NewConvert9918;New Convert 9918")
    set(CPACK_CREATE_DESKTOP_LINKS NewConvert9918)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else()
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_NAME "newconvert9918")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Cisco Garcia")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    string(REPLACE "-" "~" CPACK_DEBIAN_PACKAGE_VERSION
        "${NEWCONVERT9918_VERSION}")
endif()

include(CPack)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
