# ============================================================================
# WASAPI - Windows Audio Library
#
# State-of-the-Art WASAPI Implementation:
# - IAudioClient3 low-latency shared/exclusive mode
# - Event-driven architecture (zero polling)
# - MMCSS "Pro Audio" thread priority
# - Hot-plug device monitoring (IMMNotificationClient)
# - Session lifecycle events (IAudioSessionEvents)
# - Peak metering + per-channel volume
# - Automatic sample-rate conversion (AUTOCONVERTPCM)
# - Modern C++20 (RAII, PIMPL, concepts)
#
# License: MIT
# Author: AI-Projects Team
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(wasapi
    VERSION 0.9.2
    LANGUAGES CXX
    DESCRIPTION "Windows Audio Library"
)

# ============================================================================
# C++20 Standard (Required)
# ============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Compiler Options
# ============================================================================

if(MSVC)
    add_compile_options(
        /W4                 # Warning level 4
        /WX                 # Warnings as errors
        /permissive-        # Standards conformance
        /Zc:__cplusplus     # Correct __cplusplus macro
        /utf-8              # UTF-8 source and execution charset
        /MP                 # Multi-processor compilation
    )

    # Release optimizations
    add_compile_options(
        $<$<CONFIG:Release>:/O2>        # Maximize speed
        $<$<CONFIG:Release>:/Ob2>       # Inline expansion
        $<$<CONFIG:Release>:/GL>        # Whole program optimization
        $<$<CONFIG:Release>:/fp:fast>   # Fast floating point
    )

    # Release linker flags
    add_link_options(
        $<$<CONFIG:Release>:/LTCG>      # Link-time code generation
        $<$<CONFIG:Release>:/OPT:REF>   # Eliminate unreferenced code
        $<$<CONFIG:Release>:/OPT:ICF>   # Identical COMDAT folding
    )
else()
    add_compile_options(
        -Wall
        -Wextra
        -Werror
        -pedantic
    )
endif()

# ============================================================================
# WASAPI Library (Static)
# ============================================================================

add_library(wasapi STATIC
    # Core
    src/device.cpp
    src/device_enumerator.cpp
    src/device_watcher.cpp
    src/logger.cpp
    # Internal support
    src/format_converter.cpp
    src/format_negotiator.cpp
    # Internal headers (listed for IDE visibility)
    src/com_initializer.h
    src/format_converter.h
    src/format_negotiator.h
    src/wasapi_log.h
)

# Public API headers
target_include_directories(wasapi
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Windows SDK libraries
target_link_libraries(wasapi
    PUBLIC
        Ole32.lib       # COM (CoInitializeEx, CoCreateInstance, etc.)
        Avrt.lib        # MMCSS (AvSetMmThreadCharacteristics, etc.)
)

# UNICODE
target_compile_definitions(wasapi
    PUBLIC
        UNICODE
        _UNICODE
)

# ============================================================================
# Installation
# ============================================================================

install(TARGETS wasapi
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/wasapi
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# ============================================================================
# Options
# ============================================================================

option(WASAPI_BUILD_EXAMPLES "Build example programs" OFF)
option(WASAPI_BUILD_TESTS    "Build unit tests"       OFF)

# ============================================================================
# Examples (Optional)
# ============================================================================

if(WASAPI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================================================
# Tests (Optional)
# ============================================================================

if(WASAPI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Configuration Summary
# ============================================================================

message(STATUS "========================================")
message(STATUS "WASAPI Configuration")
message(STATUS "========================================")
message(STATUS "Version:        ${PROJECT_VERSION}")
message(STATUS "Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:   C++${CMAKE_CXX_STANDARD}")
if(MSVC)
    message(STATUS "Compiler:       MSVC ${MSVC_VERSION}")
else()
    message(STATUS "Compiler:       ${CMAKE_CXX_COMPILER_ID}")
endif()
message(STATUS "Build Examples: ${WASAPI_BUILD_EXAMPLES}")
message(STATUS "Build Tests:    ${WASAPI_BUILD_TESTS}")
message(STATUS "========================================")
