cmake_minimum_required(VERSION 3.24)

include(config/ProductConfig.cmake)
project(LinearCore VERSION 0.1.0 LANGUAGES CXX)

option(LINEAR_BUILD_TESTS "Build the dependency-free core tests" ON)
option(LINEAR_BUILD_BENCHMARKS "Build the core benchmark executable" ON)
option(LINEAR_BUILD_ADOBE_PLUGIN "Build the native Adobe effect plug-in" OFF)
option(LINEAR_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckIPOSupported)
check_ipo_supported(RESULT LINEAR_IPO_SUPPORTED OUTPUT LINEAR_IPO_ERROR)
if(LINEAR_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

configure_file(config/ProductConfig.h.in generated/linear/ProductConfig.h @ONLY)
configure_file(config/VersionConfig.h.in generated/linear/VersionConfig.h @ONLY)

add_library(linear_core
    core/src/Color.cpp
    core/src/Pixel.cpp
    core/src/Host.cpp
    core/src/Mask.cpp
    core/src/Regions.cpp
    core/src/Detection.cpp
    core/src/Tracking.cpp
    core/src/Stabilization.cpp
    core/src/PathGeometry.cpp
    core/src/TextRaster.cpp
    core/src/Graphics.cpp
    core/src/Labels.cpp
)
target_include_directories(linear_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/core/include"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_compile_features(linear_core PUBLIC cxx_std_17)

if(WIN32)
    target_link_libraries(linear_core PUBLIC gdi32)
endif()

if(MSVC)
    target_compile_options(linear_core PRIVATE /W4 /permissive- /EHsc /utf-8)
    if(LINEAR_WARNINGS_AS_ERRORS)
        target_compile_options(linear_core PRIVATE /WX)
    endif()
else()
    target_compile_options(linear_core PRIVATE -Wall -Wextra -Wpedantic)
    if(LINEAR_WARNINGS_AS_ERRORS)
        target_compile_options(linear_core PRIVATE -Werror)
    endif()
endif()

if(LINEAR_BUILD_TESTS)
    enable_testing()
    add_executable(linear_tests tests/CoreTests.cpp)
    target_link_libraries(linear_tests PRIVATE linear_core)
    add_test(NAME linear_core_tests COMMAND linear_tests)
endif()

if(LINEAR_BUILD_BENCHMARKS)
    add_executable(linear_bench benchmarks/CoreBenchmark.cpp)
    target_link_libraries(linear_bench PRIVATE linear_core)
endif()

if(LINEAR_BUILD_ADOBE_PLUGIN)
    if(NOT DEFINED ENV{ADOBE_AE_SDK_ROOT} OR "$ENV{ADOBE_AE_SDK_ROOT}" STREQUAL "")
        message(FATAL_ERROR
            "LINEAR_BUILD_ADOBE_PLUGIN=ON requires ADOBE_AE_SDK_ROOT to point "
            "to an official, locally installed After Effects C++ SDK. "
            "Linear never downloads or redistributes Adobe SDK headers.")
    endif()
    set(LINEAR_AE_SDK_ROOT "$ENV{ADOBE_AE_SDK_ROOT}")
    if(NOT EXISTS "${LINEAR_AE_SDK_ROOT}/Examples/Headers/AE_Effect.h")
        message(FATAL_ERROR
            "ADOBE_AE_SDK_ROOT='${LINEAR_AE_SDK_ROOT}' is invalid: "
            "Examples/Headers/AE_Effect.h was not found.")
    endif()
    add_subdirectory(plugin)
endif()
