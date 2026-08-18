# SPDX-FileCopyrightText: 2026 Kushview, LLC
# SPDX-FileCopyrightText: 2026 Fooboody Studio
# SPDX-License-Identifier: GPL-3.0-or-later

set(WIN_SPARKLE_VERSION "0.9.2" CACHE STRING "WinSparkle version to download")

# A fork must supply its own signed update feed and public key before enabling updates.
set(WIN_SPARKLE_FEED_URL ""
    CACHE STRING "Fooboody Studio WinSparkle feed URL")
set(WIN_SPARKLE_PUBLIC_KEY ""
    CACHE STRING "Fooboody Studio WinSparkle EdDSA public key")

FetchContent_Declare(
    winsparkle
    URL https://github.com/vslavik/winsparkle/releases/download/v${WIN_SPARKLE_VERSION}/WinSparkle-${WIN_SPARKLE_VERSION}.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(winsparkle)

message(STATUS "Searching for Sparkle in: ${winsparkle_SOURCE_DIR}")

set(WIN_SPARKLE_DLL "${winsparkle_SOURCE_DIR}/x64/Release/WinSparkle.dll")

add_library(WinSparkle::WinSparkle SHARED IMPORTED)
set_target_properties(WinSparkle::WinSparkle PROPERTIES
    IMPORTED_LOCATION "${winsparkle_SOURCE_DIR}/x64/Release/WinSparkle.dll"
    IMPORTED_IMPLIB "${winsparkle_SOURCE_DIR}/x64/Release/WinSparkle.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${winsparkle_SOURCE_DIR}/include"
)

message(STATUS "WinSparkle library: ${winsparkle_SOURCE_DIR}/x64/Release/WinSparkle.lib")
message(STATUS "WinSparkle include: ${winsparkle_SOURCE_DIR}/include")
