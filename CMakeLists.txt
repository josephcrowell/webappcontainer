# Copyright (C) 2026 Joseph Crowell.
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/compile_commands.json")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_SOURCE_DIR}/compile_commands.json"
    )
endif()

project(webappcontainer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Gui WebEngineWidgets LinguistTools)
find_package(Qt6 REQUIRED COMPONENTS Core Gui WebEngineWidgets LinguistTools)

set(TS_FILES webappcontainer_en_US.ts)

set(PROJECT_SOURCES
        main.cpp
        browserwindow.cpp browserwindow.h browserwindow.ui
        certificateerrordialog.ui
        downloadmanagerwidget.cpp downloadmanagerwidget.h downloadmanagerwidget.ui
        downloadwidget.cpp downloadwidget.h downloadwidget.ui
        passworddialog.ui
        webpage.cpp webpage.h
        webpopupwindow.cpp webpopupwindow.h
        webview.cpp webview.h
        webauthdialog.cpp webauthdialog.h webauthdialog.ui
        ${TS_FILES}
)

qt_add_executable(webappcontainer
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET webappcontainer APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

target_link_libraries(webappcontainer PUBLIC
    Qt::Core
    Qt::Gui
    Qt::WebEngineWidgets
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.webappcontainer)
endif()
set_target_properties(webappcontainer PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS webappcontainer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(webappcontainer)
endif()
