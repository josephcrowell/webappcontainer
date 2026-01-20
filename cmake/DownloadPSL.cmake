# Copyright (C) 2026 Joseph Crowell.
# SPDX-License-Identifier: GPL-2.0-or-later

file(DOWNLOAD
    "${PSL_URL}"
    "${PSL_FILE}"
    SHOW_PROGRESS
    STATUS DOWNLOAD_STATUS
)
list(GET DOWNLOAD_STATUS 0 DOWNLOAD_ERROR)
if(DOWNLOAD_ERROR)
    message(FATAL_ERROR "Failed to download Public Suffix List: ${DOWNLOAD_STATUS}")
else()
    message(STATUS "Public Suffix List updated successfully")
endif()
