# Copyright (C) 2026 Joseph Crowell.
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)

project(webappcontainer
    VERSION 0.1
    DESCRIPTION "A lightweight web app container"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    WebEngineWidgets
    LinguistTools
    Svg
)

qt_standard_project_setup()

set(PSL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/public_suffix_list.dat")
set(PSL_URL "https://publicsuffix.org/list/public_suffix_list.dat")

if(NOT EXISTS "${PSL_FILE}")
    message(STATUS "Downloading Public Suffix List...")
    file(DOWNLOAD
        "${PSL_URL}"
        "${PSL_FILE}"
        SHOW_PROGRESS
        STATUS PSL_DOWNLOAD_STATUS
    )
    list(GET PSL_DOWNLOAD_STATUS 0 PSL_DOWNLOAD_ERROR)
    if(PSL_DOWNLOAD_ERROR)
        message(WARNING "Failed to download Public Suffix List: ${PSL_DOWNLOAD_STATUS}")
    else()
        message(STATUS "Public Suffix List downloaded successfully")
    endif()
endif()

add_custom_target(update-psl
    COMMAND ${CMAKE_COMMAND} -E remove -f "${PSL_FILE}"
    COMMAND ${CMAKE_COMMAND}
        -DPSL_URL="${PSL_URL}"
        -DPSL_FILE="${PSL_FILE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DownloadPSL.cmake"
    COMMENT "Updating Public Suffix List..."
)

# Widevine CDM for DRM support
option(ENABLE_WIDEVINE "Download and enable Widevine CDM for DRM support" ON)
set(WIDEVINE_VERSION "4.10.2934.0" CACHE STRING "Widevine CDM version to download")
set(WIDEVINE_DIR "${CMAKE_BINARY_DIR}/widevine" CACHE PATH "Widevine CDM installation directory")

if(ENABLE_WIDEVINE)
    # Determine library name based on platform
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(WIDEVINE_LIB_NAME "libwidevinecdm.so")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(WIDEVINE_LIB_NAME "libwidevinecdm.dylib")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(WIDEVINE_LIB_NAME "widevinecdm.dll")
    endif()

    set(WIDEVINE_LIB "${WIDEVINE_DIR}/${WIDEVINE_LIB_NAME}")

    if(NOT EXISTS "${WIDEVINE_LIB}")
        message(STATUS "Downloading Widevine CDM ${WIDEVINE_VERSION}...")
        execute_process(
            COMMAND ${CMAKE_COMMAND}
                -DWIDEVINE_VERSION=${WIDEVINE_VERSION}
                -DWIDEVINE_DIR=${WIDEVINE_DIR}
                -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
                -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DownloadWidevine.cmake"
            RESULT_VARIABLE WIDEVINE_DOWNLOAD_RESULT
        )
        if(WIDEVINE_DOWNLOAD_RESULT)
            message(WARNING "Failed to download Widevine CDM. DRM content will not be available.")
            set(ENABLE_WIDEVINE OFF)
        endif()
    else()
        message(STATUS "Widevine CDM already present at ${WIDEVINE_LIB}")
    endif()

    if(ENABLE_WIDEVINE)
        # Install Widevine CDM library to webappcontainer-specific lib directory
        install(FILES "${WIDEVINE_LIB}"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/webappcontainer"
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )

        # Install Widevine metadata files if they exist
        if(EXISTS "${WIDEVINE_DIR}/manifest.json")
            install(FILES "${WIDEVINE_DIR}/manifest.json"
                DESTINATION "${CMAKE_INSTALL_LIBDIR}/webappcontainer"
                PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
            )
        endif()
        if(EXISTS "${WIDEVINE_DIR}/LICENSE")
            install(FILES "${WIDEVINE_DIR}/LICENSE"
                DESTINATION "${CMAKE_INSTALL_LIBDIR}/webappcontainer"
                PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
            )
        endif()

        # Tell the application to look for Widevine relative to the executable
        add_compile_definitions(WIDEVINE_CDM_ENABLED)
    endif()
endif()

# Custom target to update/re-download Widevine CDM
add_custom_target(update-widevine
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${WIDEVINE_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DWIDEVINE_VERSION=${WIDEVINE_VERSION}
        -DWIDEVINE_DIR=${WIDEVINE_DIR}
        -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
        -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DownloadWidevine.cmake"
    COMMENT "Updating Widevine CDM..."
)

set(PROJECT_SOURCES
    main.cpp
    browserwindow.cpp browserwindow.h browserwindow.ui
    certificateerrordialog.ui
    downloadmanagerwidget.cpp downloadmanagerwidget.h downloadmanagerwidget.ui
    downloadwidget.cpp downloadwidget.h downloadwidget.ui
    passworddialog.ui
    publicsuffixlist.cpp publicsuffixlist.h
    webpage.cpp webpage.h
    webpopupwindow.cpp webpopupwindow.h
    webview.cpp webview.h
    webauthdialog.cpp webauthdialog.h webauthdialog.ui
)

qt_add_executable(webappcontainer
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

target_link_libraries(webappcontainer PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::Svg
)

# Don't print debug messages in release mode
target_compile_definitions(webappcontainer PRIVATE
    $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>
)

set_target_properties(webappcontainer PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

include(GNUInstallDirs)

install(TARGETS webappcontainer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET webappcontainer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json"
    COMMENT "Updating compile_commands.json symlink in source root"
)

qt_finalize_executable(webappcontainer)

# Testing
option(BUILD_TESTING "Build the testing tree" ON)
if(BUILD_TESTING)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    # Widevine/DRM test
    qt_add_executable(tst_widevine
        tests/tst_widevine.cpp
    )
    target_link_libraries(tst_widevine PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::WebEngineWidgets
    )

    # Pass Widevine path to test if available
    if(ENABLE_WIDEVINE AND EXISTS "${WIDEVINE_LIB}")
        target_compile_definitions(tst_widevine PRIVATE
            WIDEVINE_CDM_PATH="${WIDEVINE_LIB}"
        )
    endif()

    add_test(NAME tst_widevine COMMAND tst_widevine)

    # Set environment for test to find Widevine
    if(ENABLE_WIDEVINE AND EXISTS "${WIDEVINE_LIB}")
        set_tests_properties(tst_widevine PROPERTIES
            ENVIRONMENT "QTWEBENGINE_CHROMIUM_FLAGS=--widevine-cdm-path=${WIDEVINE_LIB}"
        )
    endif()

    # Service Worker test
    qt_add_executable(tst_serviceworker
        tests/tst_serviceworker.cpp
    )
    target_link_libraries(tst_serviceworker PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::WebEngineWidgets
    )

    add_test(NAME tst_serviceworker COMMAND tst_serviceworker)
endif()
