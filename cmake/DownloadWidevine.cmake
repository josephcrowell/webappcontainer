# Copyright (C) 2026 Joseph Crowell.
# SPDX-License-Identifier: GPL-2.0-or-later
#
# CMake script to download and extract Widevine CDM for DRM support
# Downloads Widevine CDM as a Chrome component (CRX3 format)

# Widevine CDM version and download hash
# Update these periodically by checking:
# https://aur.archlinux.org/cgit/aur.git/tree/PKGBUILD?h=chromium-widevine
if(NOT DEFINED WIDEVINE_VERSION)
    set(WIDEVINE_VERSION "4.10.2934.0")
endif()
if(NOT DEFINED WIDEVINE_HASH)
    set(WIDEVINE_HASH "accssjtqfpf5qicscrptql4jyyxa_4.10.2934.0")
endif()
if(NOT DEFINED WIDEVINE_FILE_HASH)
    set(WIDEVINE_FILE_HASH "ph722a3wl2goebkpserszm6bde")
endif()

# Widevine Chrome extension ID
set(WIDEVINE_EXTENSION_ID "oimompecagnajdejgnnjijobebaeigek")

# Determine platform
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        set(WIDEVINE_PLATFORM "linux")
        set(WIDEVINE_LIB_PATH "_platform_specific/linux_x64")
    else()
        message(FATAL_ERROR "Unsupported Linux architecture for Widevine: ${CMAKE_SYSTEM_PROCESSOR}\n"
                "Only x86_64 is currently supported via Chrome component download.")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}\n"
            "This script currently only supports Linux x86_64.\n"
            "For other platforms, manually place the Widevine library in ${WIDEVINE_DIR}/")
endif()

# Construct the CRX3 download URL
# Format: https://www.google.com/dl/release2/chrome_component/{hash}/{extension_id}_{version}_{platform}_{file_hash}.crx3
set(WIDEVINE_URL "https://www.google.com/dl/release2/chrome_component/${WIDEVINE_HASH}/${WIDEVINE_EXTENSION_ID}_${WIDEVINE_VERSION}_${WIDEVINE_PLATFORM}_${WIDEVINE_FILE_HASH}.crx3")
set(WIDEVINE_CRX "${WIDEVINE_DIR}/widevine.crx3")

message(STATUS "Downloading Widevine CDM ${WIDEVINE_VERSION}...")
message(STATUS "URL: ${WIDEVINE_URL}")

file(DOWNLOAD
    "${WIDEVINE_URL}"
    "${WIDEVINE_CRX}"
    SHOW_PROGRESS
    STATUS DOWNLOAD_STATUS
    TLS_VERIFY ON
)

list(GET DOWNLOAD_STATUS 0 DOWNLOAD_ERROR)
if(DOWNLOAD_ERROR)
    message(FATAL_ERROR "Failed to download Widevine CDM: ${DOWNLOAD_STATUS}\n"
            "URL: ${WIDEVINE_URL}\n"
            "\n"
            "The download URL may have changed. Check for updated version/hash at:\n"
            "https://aur.archlinux.org/cgit/aur.git/tree/PKGBUILD?h=chromium-widevine")
endif()

message(STATUS "Extracting Widevine CDM...")

# CRX3 files are ZIP files with a header, so we can extract them directly
file(ARCHIVE_EXTRACT
    INPUT "${WIDEVINE_CRX}"
    DESTINATION "${WIDEVINE_DIR}"
)

# Clean up CRX file
file(REMOVE "${WIDEVINE_CRX}")

# Verify the library exists (it's in a subdirectory in the CRX)
set(WIDEVINE_LIB_EXTRACTED "${WIDEVINE_DIR}/${WIDEVINE_LIB_PATH}/libwidevinecdm.so")

if(NOT EXISTS "${WIDEVINE_LIB_EXTRACTED}")
    message(FATAL_ERROR "Widevine CDM library not found after extraction: ${WIDEVINE_LIB_EXTRACTED}\n"
            "Expected location: ${WIDEVINE_LIB_PATH}/libwidevinecdm.so")
endif()

# Copy the library to the root of WIDEVINE_DIR for easier access
file(COPY "${WIDEVINE_LIB_EXTRACTED}" DESTINATION "${WIDEVINE_DIR}")

set(WIDEVINE_LIB "${WIDEVINE_DIR}/libwidevinecdm.so")

message(STATUS "Widevine CDM ${WIDEVINE_VERSION} installed successfully to ${WIDEVINE_DIR}")
