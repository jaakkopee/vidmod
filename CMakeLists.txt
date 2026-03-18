cmake_minimum_required(VERSION 3.15)
project(VidMod VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(SFML 3 COMPONENTS System Window Graphics Audio REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW REQUIRED IMPORTED_TARGET fftw3)
pkg_check_modules(OPENCV REQUIRED IMPORTED_TARGET opencv4)
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)
find_package(nlohmann_json 3.2.0 REQUIRED)

# OpenMP for macOS (homebrew libomp)
if(APPLE)
    set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp")
    set(OpenMP_C_LIB_NAMES "omp")
    set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
    set(OpenMP_CXX_LIB_NAMES "omp")
    set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")
    include_directories("/opt/homebrew/opt/libomp/include")
    link_directories("/opt/homebrew/opt/libomp/lib")
endif()
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

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
    src/CircleQuiltEffect.cpp
    src/EdgeInkEffect.cpp
    src/CAGlowEffect.cpp
    src/NeuralTileEffect.cpp
    src/NeuralCircleEffect.cpp
    src/EffectChain.cpp
    src/VideoProcessor.cpp
    src/GUI.cpp
    src/ParameterAutomation.cpp
    src/AutomationWindow.cpp
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

if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} OpenMP::OpenMP_CXX)
endif()

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
