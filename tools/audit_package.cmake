foreach(required_variable ROOT GUI_REL PLATFORM)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

cmake_path(ABSOLUTE_PATH ROOT NORMALIZE OUTPUT_VARIABLE package_root)
set(gui_path "${package_root}/${GUI_REL}")
if(NOT EXISTS "${gui_path}")
    message(FATAL_ERROR "Packaged GUI is missing: ${gui_path}")
endif()
if(DEFINED CLI_REL AND NOT CLI_REL STREQUAL "")
    if(NOT EXISTS "${package_root}/${CLI_REL}")
        message(FATAL_ERROR "Packaged CLI is missing: ${package_root}/${CLI_REL}")
    endif()
endif()

foreach(notice LICENSE NOTICE.md THIRD_PARTY_NOTICES.md)
    if(NOT EXISTS "${package_root}/${notice}")
        message(FATAL_ERROR "Required package notice is missing: ${notice}")
    endif()
endforeach()
if(DEFINED REQUIRE_QT_LICENSES AND REQUIRE_QT_LICENSES)
    foreach(qt_license LICENSES/LGPL-3.0.txt LICENSES/GPL-3.0.txt)
        if(NOT EXISTS "${package_root}/${qt_license}")
            message(FATAL_ERROR
                "Required Qt redistribution license is missing: ${qt_license}")
        endif()
    endforeach()
endif()

file(GLOB_RECURSE package_files
    LIST_DIRECTORIES FALSE
    RELATIVE "${package_root}"
    "${package_root}/*")
if(NOT package_files)
    message(FATAL_ERROR "Package tree is empty: ${package_root}")
endif()

set(forbidden_patterns
    "(^|/)CMakeFiles(/|$)"
    "(^|/)CMakeCache.txt$"
    "(^|/)compile_commands.json$"
    "(^|/)plugins/qmltooling(/|$)"
    "(^|/).+\\.obj$"
    "(^|/).+\\.o$"
    "(^|/).+\\.pdb$"
    "(^|/)Qt6[^/]*d\\.dll$")
foreach(package_file IN LISTS package_files)
    string(REPLACE "\\\\" "/" normalized_file "${package_file}")
    foreach(pattern IN LISTS forbidden_patterns)
        if(normalized_file MATCHES "${pattern}")
            message(FATAL_ERROR
                "Development or debug file found in package: ${normalized_file}")
        endif()
    endforeach()
endforeach()

if(PLATFORM STREQUAL "windows")
    set(runtime_candidates
        "bin/Qt6Core.dll"
        "plugins/platforms/qwindows.dll")
elseif(PLATFORM STREQUAL "macos")
    set(runtime_candidates
        "RetroVDPStudio.app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore"
        "RetroVDPStudio.app/Contents/PlugIns/platforms/libqcocoa.dylib")
elseif(PLATFORM STREQUAL "linux")
    set(runtime_candidates
        "lib/libQt6Core.so.6"
        "plugins/platforms/libqxcb.so")
else()
    message(FATAL_ERROR "Unknown PLATFORM value: ${PLATFORM}")
endif()

foreach(runtime_file IN LISTS runtime_candidates)
    if(NOT EXISTS "${package_root}/${runtime_file}")
        message(FATAL_ERROR "Required runtime file is missing: ${runtime_file}")
    endif()
endforeach()

list(LENGTH package_files file_count)
message(STATUS
    "Audited ${file_count} files in ${package_root}; runtime and notices present")
