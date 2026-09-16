# Copyright (C) 2026 Joseph Crowell
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)
project(webappcontainer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
file(CREATE_LINK
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json"
    SYMBOLIC
)

option(ENABLE_WIDEVINE "Enable Widevine CDM support" ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS
    Core
    DBus
    Gui
    LinguistTools
    Network
    Qml
    Quick
    QuickControls2
    Svg
    WebEngineCore
    WebEngineQuick
    Widgets
)

qt_standard_project_setup()
# QML sources live in qml/ below the module root; let their implicit imports
# resolve the same embedded module as explicit imports after installation.
qt_policy(SET QTP0004 NEW)
include(GNUInstallDirs)

set(PSL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/public_suffix_list.dat")
set(PSL_URL "https://publicsuffix.org/list/public_suffix_list.dat")
if(NOT EXISTS "${PSL_FILE}")
    file(DOWNLOAD "${PSL_URL}" "${PSL_FILE}"
        STATUS PSL_DOWNLOAD_STATUS TLS_VERIFY ON)
    list(GET PSL_DOWNLOAD_STATUS 0 PSL_DOWNLOAD_ERROR)
    if(PSL_DOWNLOAD_ERROR)
        message(FATAL_ERROR "Failed to download Public Suffix List: ${PSL_DOWNLOAD_STATUS}")
    endif()
endif()
add_custom_target(update-psl
    COMMAND ${CMAKE_COMMAND}
        -DPSL_URL=${PSL_URL}
        -DPSL_FILE=${PSL_FILE}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DownloadPSL.cmake"
    COMMENT "Updating Public Suffix List"
)

set(WIDEVINE_AVAILABLE FALSE)
if(ENABLE_WIDEVINE)
    set(WIDEVINE_DIR "${CMAKE_BINARY_DIR}/widevine")
    set(WIDEVINE_LIB "${WIDEVINE_DIR}/libwidevinecdm.so")
    set(WIDEVINE_MANIFEST "${WIDEVINE_DIR}/manifest.json")

    set(SYSTEM_WIDEVINE_PATHS
        "/usr/lib/chromium/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so"
        "/usr/lib/chromium-browser/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so"
        "/usr/lib64/chromium-browser/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so"
        "/usr/lib/chromium-browser/WidevineCdm/libwidevinecdm.so"
        "/opt/google/chrome/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so"
        "/opt/google/chrome-beta/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so"
    )

    set(SYSTEM_WIDEVINE_FOUND "")
    foreach(WV_PATH ${SYSTEM_WIDEVINE_PATHS})
        if(EXISTS "${WV_PATH}")
            set(SYSTEM_WIDEVINE_FOUND "${WV_PATH}")
            break()
        endif()
    endforeach()

    if(SYSTEM_WIDEVINE_FOUND)
        get_filename_component(WIDEVINE_LIB "${SYSTEM_WIDEVINE_FOUND}" ABSOLUTE)
        get_filename_component(SYSTEM_WV_DIR "${WIDEVINE_LIB}" DIRECTORY)
        if(EXISTS "${SYSTEM_WV_DIR}/manifest.json")
            set(WIDEVINE_MANIFEST "${SYSTEM_WV_DIR}/manifest.json")
        endif()
    else()
        execute_process(
            COMMAND ${CMAKE_COMMAND}
                -DWIDEVINE_DIR=${WIDEVINE_DIR}
                -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
                -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DownloadWidevine.cmake"
            RESULT_VARIABLE WIDEVINE_DOWNLOAD_RESULT
        )
        if(WIDEVINE_DOWNLOAD_RESULT)
            message(WARNING "Failed to download Widevine CDM; DRM tests will be skipped")
        endif()
    endif()

    if(EXISTS "${WIDEVINE_LIB}")
        set(WIDEVINE_AVAILABLE TRUE)
        install(FILES "${WIDEVINE_LIB}"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/webappcontainer"
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
        if(EXISTS "${WIDEVINE_MANIFEST}")
            install(FILES "${WIDEVINE_MANIFEST}"
                DESTINATION "${CMAKE_INSTALL_LIBDIR}/webappcontainer"
            )
        endif()
    endif()
endif()

add_custom_target(update-widevine
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/widevine"
    COMMAND ${CMAKE_COMMAND}
        -DWIDEVINE_DIR=${CMAKE_BINARY_DIR}/widevine
        -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
        -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DownloadWidevine.cmake"
    COMMENT "Updating Widevine CDM"
)

add_library(webappcontainer_core STATIC
    core/startup.cpp
    core/startup.h
)
target_link_libraries(webappcontainer_core PUBLIC Qt6::Core)
target_include_directories(webappcontainer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_executable(webappcontainer
    MANUAL_FINALIZATION
    quick/main.cpp
)

set_source_files_properties(public_suffix_list.dat PROPERTIES
    QT_RESOURCE_ALIAS public_suffix_list.dat)
set_source_files_properties(resources/default-icon.svg PROPERTIES
    QT_RESOURCE_ALIAS default.svg)
set_source_files_properties(resources/push-subscription-persistence.js PROPERTIES
    QT_RESOURCE_ALIAS push-subscription-persistence.js)
qt_add_resources(webappcontainer webappcontainer_data
    PREFIX "/data"
    FILES public_suffix_list.dat
)
qt_add_resources(webappcontainer webappcontainer_icons
    PREFIX "/icons"
    FILES resources/default-icon.svg
)
qt_add_resources(webappcontainer webappcontainer_scripts
    PREFIX "/scripts"
    FILES resources/push-subscription-persistence.js
)

qt_add_qml_module(webappcontainer
    URI WebAppContainer
    VERSION 1.0
    # Keep the embedded qmldir on QQmlEngine's default import path. The build
    # directory must not be needed to discover Main after installation.
    RESOURCE_PREFIX /qt/qml
    SOURCES
        app/appcontroller.cpp
        app/appcontroller.h
        app/downloadcontroller.cpp
        app/downloadcontroller.h
        app/downloadmodel.cpp
        app/downloadmodel.h
        app/iconloader.cpp
        app/iconloader.h
        app/launchconfiguration.cpp
        app/launchconfiguration.h
        app/navigationpolicy.cpp
        app/navigationpolicy.h
        app/notificationmodel.cpp
        app/notificationmodel.h
        app/notificationpresenter.cpp
        app/notificationpresenter.h
        app/profileservice.cpp
        app/profileservice.h
        app/securitybridge.h
        app/securitybridge.cpp
        app/settingsstore.cpp
        app/settingsstore.h
        publicsuffixlist.cpp
        publicsuffixlist.h
    QML_FILES
        qml/AppWebView.qml
        qml/DownloadDelegate.qml
        qml/DownloadsWindow.qml
        qml/Main.qml
        qml/PopupWindow.qml
        qml/PopupManager.qml
        qml/RequestDialogs.qml
        qml/TrayController.qml
)

target_include_directories(webappcontainer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app
)
target_link_libraries(webappcontainer PRIVATE
    webappcontainer_core
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
    Qt6::WebEngineCore
    Qt6::WebEngineQuick
    Qt6::Widgets
)

if(WIDEVINE_AVAILABLE)
    target_compile_definitions(webappcontainer PRIVATE
        WIDEVINE_CDM_ENABLED
        WIDEVINE_CDM_PATH="${WIDEVINE_LIB}"
    )
endif()

set(TS_FILES
    translations/webappcontainer_cs_CZ.ts
    translations/webappcontainer_de_DE.ts
    translations/webappcontainer_en_US.ts
    translations/webappcontainer_es_ES.ts
    translations/webappcontainer_fr_FR.ts
    translations/webappcontainer_it_IT.ts
    translations/webappcontainer_ja_JP.ts
    translations/webappcontainer_ko_KR.ts
    translations/webappcontainer_nb_NO.ts
    translations/webappcontainer_nl_NL.ts
    translations/webappcontainer_pl_PL.ts
    translations/webappcontainer_pt_BR.ts
    translations/webappcontainer_pt_PT.ts
    translations/webappcontainer_ru_RU.ts
    translations/webappcontainer_sv_SE.ts
    translations/webappcontainer_tr_TR.ts
    translations/webappcontainer_uk_UA.ts
    translations/webappcontainer_zh_CN.ts
    translations/webappcontainer_zh_TW.ts
)
qt_add_translations(webappcontainer TS_FILES ${TS_FILES})

set_target_properties(webappcontainer PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

install(TARGETS webappcontainer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
qt_finalize_executable(webappcontainer)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Test)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XCB_ICCCM REQUIRED IMPORTED_TARGET xcb xcb-icccm)

    add_test(NAME tst_deployed_qml
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_deployed_qml.py"
            $<TARGET_FILE:webappcontainer>
    )

    qt_add_executable(tst_push_profile_helper
        tests/push_profile_helper.cpp
    )
    qt_add_resources(tst_push_profile_helper push_helper_resources
        PREFIX "/scripts"
        BASE "resources"
        FILES resources/push-subscription-persistence.js
    )
    target_include_directories(tst_push_profile_helper PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(tst_push_profile_helper PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::WebEngineQuick
        Qt6::Widgets
    )
    add_test(NAME tst_push_persistence
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_push_persistence.py"
            $<TARGET_FILE:tst_push_profile_helper>
    )
    set_tests_properties(tst_push_persistence PROPERTIES
        TIMEOUT 180
        RUN_SERIAL TRUE
        LABELS "integration;push"
    )
    set_tests_properties(tst_deployed_qml PROPERTIES
        TIMEOUT 50
        RUN_SERIAL TRUE
        LABELS "integration;deployment"
    )

    qt_add_executable(tst_startup tests/tst_startup.cpp)
    target_link_libraries(tst_startup PRIVATE webappcontainer_core Qt6::Core Qt6::Test)
    add_test(NAME tst_startup COMMAND tst_startup)

    qt_add_executable(tst_appcontroller
        tests/tst_appcontroller.cpp
        app/appcontroller.cpp
        app/appcontroller.h
    )
    target_include_directories(tst_appcontroller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_appcontroller PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
        Qt6::Widgets
    )
    add_test(NAME tst_appcontroller COMMAND tst_appcontroller)

    qt_add_executable(tst_iconloader
        tests/tst_iconloader.cpp
        app/iconloader.cpp
        app/iconloader.h
    )
    target_include_directories(tst_iconloader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_iconloader PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Svg
        Qt6::Test
        Qt6::Widgets
    )
    add_test(NAME tst_iconloader COMMAND tst_iconloader)

    qt_add_executable(tst_downloadmodel
        tests/tst_downloadmodel.cpp
        app/downloadmodel.cpp
        app/downloadmodel.h
    )
    target_include_directories(tst_downloadmodel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_downloadmodel PRIVATE Qt6::Core Qt6::Qml Qt6::Test)
    add_test(NAME tst_downloadmodel COMMAND tst_downloadmodel)

    qt_add_executable(tst_navigationpolicy
        tests/tst_navigationpolicy.cpp
        app/navigationpolicy.cpp
        app/navigationpolicy.h
        publicsuffixlist.cpp
        publicsuffixlist.h
    )
    qt_add_resources(tst_navigationpolicy navigation_policy_data
        PREFIX "/data"
        FILES public_suffix_list.dat
    )
    target_include_directories(tst_navigationpolicy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_navigationpolicy PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Test)
    add_test(NAME tst_navigationpolicy COMMAND tst_navigationpolicy)

    qt_add_executable(tst_settingsstore
        tests/tst_settingsstore.cpp
        app/settingsstore.cpp
        app/settingsstore.h
    )
    target_include_directories(tst_settingsstore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_settingsstore PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Test
        Qt6::Widgets
    )
    add_test(NAME tst_settingsstore COMMAND tst_settingsstore)

    qt_add_executable(tst_x11geometry
        tests/tst_x11geometry.cpp
        app/settingsstore.cpp
        app/settingsstore.h
    )
    target_include_directories(tst_x11geometry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_x11geometry PRIVATE
        PkgConfig::XCB_ICCCM
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Test
        Qt6::Widgets
    )
    add_test(NAME tst_x11geometry COMMAND tst_x11geometry)

    qt_add_executable(tst_quickwebengine
        tests/tst_quickwebengine.cpp
        app/downloadcontroller.cpp
        app/downloadcontroller.h
        app/downloadmodel.cpp
        app/downloadmodel.h
        app/notificationmodel.cpp
        app/notificationmodel.h
    )
    target_link_libraries(tst_quickwebengine PRIVATE
        webappcontainer_core
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
        Qt6::WebEngineQuick
        Qt6::Widgets
    )
    target_compile_definitions(tst_quickwebengine PRIVATE
        TEST_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/resources"
    )
    if(WIDEVINE_AVAILABLE)
        target_compile_definitions(tst_quickwebengine PRIVATE
            WIDEVINE_CDM_PATH="${WIDEVINE_LIB}"
        )
    endif()
    add_test(NAME tst_quickwebengine COMMAND tst_quickwebengine)
endif()
