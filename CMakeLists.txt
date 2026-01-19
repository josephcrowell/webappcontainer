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

set(TS_FILES webappcontainer_en_US.ts)

set(PROJECT_SOURCES
    main.cpp
    browserwindow.cpp browserwindow.h browserwindow.ui
    certificateerrordialog.ui
    downloadmanagerwidget.cpp downloadmanagerwidget.h downloadmanagerwidget.ui
    downloadwidget.cpp downloadwidget.h downloadwidget.ui
    passworddialog.ui
    webpage.cpp webpage.h
    webview.cpp webview.h
    webauthdialog.cpp webauthdialog.h webauthdialog.ui
    ${TS_FILES}
)

qt_add_executable(webappcontainer
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

qt_add_lrelease(webappcontainer
    TS_FILES ${TS_FILES}
    QM_FILES_OUTPUT_VARIABLE QM_FILES
)

set_source_files_properties(${QM_FILES} PROPERTIES GENERATED TRUE)

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

if(QM_FILES)
    install(FILES ${QM_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/webappcontainer/translations)
endif()

add_custom_command(TARGET webappcontainer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json"
    COMMENT "Updating compile_commands.json symlink in source root"
)

qt_finalize_executable(webappcontainer)
