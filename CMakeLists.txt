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
