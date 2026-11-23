cmake_minimum_required(VERSION 3.24)
project(SpectrumWall VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED)
include(FetchContent)
FetchContent_Declare(
    miniaudio
    URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    FetchContent_Populate(miniaudio)
endif()

add_executable(SpectrumWall
    src/main.cpp
    src/AudioCapture.cpp
    src/SpectrumAnalyzer.cpp
    src/WallRenderer.cpp
)
target_include_directories(SpectrumWall PRIVATE src "${miniaudio_SOURCE_DIR}")
target_link_libraries(SpectrumWall PRIVATE SDL3::SDL3)
if(WIN32)
    target_compile_definitions(SpectrumWall PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(MSVC)
    target_compile_options(SpectrumWall PRIVATE /W4 /EHsc)
    target_compile_definitions(SpectrumWall PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(SpectrumWall PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32 AND TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET SpectrumWall POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "$<TARGET_FILE:SDL3::SDL3-shared>"
          "$<TARGET_FILE_DIR:SpectrumWall>"
        COMMENT "Copying SDL3 runtime library"
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(SpectrumWallBandTests
        tests/spectrum_bands.cpp
        src/SpectrumAnalyzer.cpp
    )
    target_include_directories(SpectrumWallBandTests PRIVATE src "${miniaudio_SOURCE_DIR}")
    if(WIN32)
        target_compile_definitions(SpectrumWallBandTests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    add_test(NAME stereo-spectrum-band-count COMMAND SpectrumWallBandTests)
endif()
