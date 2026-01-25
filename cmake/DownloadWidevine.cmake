# Copyright (C) 2026 Joseph Crowell.
# SPDX-License-Identifier: GPL-2.0-or-later
#
# CMake script to download and extract Widevine CDM for DRM support
# Downloads Google Chrome deb package and extracts libwidevinecdm.so

# Determine platform
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        set(WIDEVINE_PLATFORM "linux-x64")
    else()
        message(FATAL_ERROR "Unsupported Linux architecture for Widevine: ${CMAKE_SYSTEM_PROCESSOR}\n"
                "Only x86_64 is currently supported via Google Chrome deb package.")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}\n"
            "This script currently only supports Linux x86_64.\n"
            "For other platforms, manually place the Widevine library in ${WIDEVINE_DIR}/")
endif()

# Google Chrome deb download URL
set(CHROME_URL "https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb")
set(CHROME_DEB "${WIDEVINE_DIR}/google-chrome-stable_current_amd64.deb")

message(STATUS "Downloading Google Chrome for Widevine CDM...")
message(STATUS "URL: ${CHROME_URL}")

file(DOWNLOAD
    "${CHROME_URL}"
    "${CHROME_DEB}"
    SHOW_PROGRESS
    STATUS DOWNLOAD_STATUS
    TLS_VERIFY ON
)

list(GET DOWNLOAD_STATUS 0 DOWNLOAD_ERROR)
if(DOWNLOAD_ERROR)
    message(FATAL_ERROR "Failed to download Google Chrome: ${DOWNLOAD_STATUS}\n"
            "URL: ${CHROME_URL}\n"
            "\n"
            "The download URL may have changed. Check the Google Chrome download page.")
endif()

message(STATUS "Extracting Google Chrome deb package...")

# Extract deb package using dpkg
execute_process(
    COMMAND dpkg -x "${CHROME_DEB}" "${WIDEVINE_DIR}"
    RESULT_VARIABLE DPKG_RESULT
    ERROR_VARIABLE DPKG_ERROR
)

if(NOT DPKG_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to extract Google Chrome deb package: ${DPKG_ERROR}\n"
            "Make sure dpkg is installed on your system.")
endif()

# Clean up deb file
file(REMOVE "${CHROME_DEB}")

# Look for libwidevinecdm.so in the extracted Chrome files
set(WIDEVINE_LIB_EXTRACTED "${WIDEVINE_DIR}/opt/google/chrome/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so")

if(NOT EXISTS "${WIDEVINE_LIB_EXTRACTED}")
    message(FATAL_ERROR "Widevine CDM library not found after extraction: ${WIDEVINE_LIB_EXTRACTED}\n"
            "Expected location: opt/google/chrome/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so")
endif()

# Copy the library to the root of WIDEVINE_DIR for easier access
file(COPY "${WIDEVINE_LIB_EXTRACTED}" DESTINATION "${WIDEVINE_DIR}")

set(WIDEVINE_LIB "${WIDEVINE_DIR}/libwidevinecdm.so")

message(STATUS "Widevine CDM extracted from Google Chrome and installed successfully to ${WIDEVINE_DIR}")
