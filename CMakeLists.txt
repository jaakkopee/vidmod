cmake_minimum_required(VERSION 3.15)
project(FFTVidMod VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(SFML 3 COMPONENTS System Window Graphics Audio REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW REQUIRED IMPORTED_TARGET fftw3)
pkg_check_modules(OPENCV REQUIRED IMPORTED_TARGET opencv4)
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)
find_package(nlohmann_json 3.2.0 REQUIRED)

# TGUI
find_package(TGUI 1 REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# Source files
set(SOURCES
    src/main.cpp
    src/CircularBuffer.cpp
    src/AudioBuffer.cpp
    src/AudioPlaylist.cpp
    src/VideoBuffer.cpp
    src/Effect.cpp
    src/FFTEffect.cpp
    src/ShadowEffect.cpp
    src/LightEffect.cpp
    src/DiffuseEffect.cpp
    src/AudioColorEffect.cpp
    src/FractalEffect.cpp
    src/EffectChain.cpp
    src/VideoProcessor.cpp
    src/GUI.cpp
)

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    SFML::System
    SFML::Window
    SFML::Graphics
    SFML::Audio
    PkgConfig::FFTW
    PkgConfig::OPENCV
    PkgConfig::SNDFILE
    tgui
    nlohmann_json::nlohmann_json
)

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Enable warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()
