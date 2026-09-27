if(NOT DEFINED ROOT)
    message(FATAL_ERROR "ROOT is required")
endif()

cmake_path(ABSOLUTE_PATH ROOT NORMALIZE OUTPUT_VARIABLE package_root)

set(required_files
    "DEBIAN/control"
    "opt/newconvert9918/bin/NewConvert9918"
    "opt/newconvert9918/bin/newconvert9918-cli"
    "opt/newconvert9918/bin/qt.conf"
    "opt/newconvert9918/lib/libQt6Core.so.6"
    "opt/newconvert9918/plugins/platforms/libqxcb.so"
    "usr/bin/NewConvert9918"
    "usr/bin/newconvert9918-cli"
    "usr/share/applications/io.github.ciscogarciafl.NewConvert9918.desktop"
    "usr/share/icons/hicolor/256x256/apps/io.github.ciscogarciafl.NewConvert9918.png"
    "usr/share/doc/newconvert9918/LICENSE"
    "usr/share/doc/newconvert9918/NOTICE.md"
    "usr/share/doc/newconvert9918/THIRD_PARTY_NOTICES.md")
foreach(required_file IN LISTS required_files)
    if(NOT EXISTS "${package_root}/${required_file}")
        message(FATAL_ERROR
            "Required Debian package file is missing: ${required_file}")
    endif()
endforeach()

set(forbidden_paths
    "apprun-hooks"
    "usr/bin/qt.conf"
    "usr/lib"
    "usr/plugins"
    "usr/qml")
foreach(forbidden_path IN LISTS forbidden_paths)
    if(EXISTS "${package_root}/${forbidden_path}")
        message(FATAL_ERROR
            "Debian package uses forbidden global path: ${forbidden_path}")
    endif()
endforeach()

file(READ "${package_root}/DEBIAN/control" control)
if(NOT control MATCHES "Installed-Size: ([1-9][0-9]*)")
    message(FATAL_ERROR "Debian control file has no positive Installed-Size")
endif()

file(READ "${package_root}/usr/bin/NewConvert9918" gui_launcher)
if(NOT gui_launcher MATCHES
        "exec /opt/newconvert9918/bin/NewConvert9918")
    message(FATAL_ERROR "GUI launcher does not use the private runtime")
endif()
file(READ "${package_root}/usr/bin/newconvert9918-cli" cli_launcher)
if(NOT cli_launcher MATCHES
        "exec /opt/newconvert9918/bin/newconvert9918-cli")
    message(FATAL_ERROR "CLI launcher does not use the private runtime")
endif()

file(GLOB_RECURSE package_entries
    LIST_DIRECTORIES TRUE
    RELATIVE "${package_root}"
    "${package_root}/*")
foreach(package_entry IN LISTS package_entries)
    string(REPLACE "\\" "/" normalized_entry "${package_entry}")
    if(normalized_entry MATCHES "^DEBIAN(/|$)" OR
       normalized_entry MATCHES "^opt/newconvert9918(/|$)" OR
       normalized_entry MATCHES
           "^usr/bin/(NewConvert9918|newconvert9918-cli)$" OR
       normalized_entry MATCHES
           "^usr/share/applications/io[.]github[.]ciscogarciafl[.]NewConvert9918[.]desktop$" OR
       normalized_entry MATCHES "^usr/share/icons/hicolor(/|$)" OR
       normalized_entry MATCHES "^usr/share/doc/newconvert9918(/|$)" OR
       normalized_entry STREQUAL "opt" OR
       normalized_entry STREQUAL "usr" OR
       normalized_entry STREQUAL "usr/bin" OR
       normalized_entry STREQUAL "usr/share" OR
       normalized_entry STREQUAL "usr/share/applications" OR
       normalized_entry STREQUAL "usr/share/icons" OR
       normalized_entry STREQUAL "usr/share/doc")
        continue()
    endif()
    message(FATAL_ERROR
        "Unexpected non-isolated Debian package path: ${normalized_entry}")
endforeach()

list(LENGTH package_entries entry_count)
message(STATUS
    "Audited ${entry_count} Debian entries; bundled runtime is isolated under /opt/newconvert9918")
