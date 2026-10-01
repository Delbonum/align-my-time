cmake_minimum_required(VERSION 3.24)

project(AlignMyTime VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(AMT_BUILD_PLUGIN "Build the JUCE/ARA plug-in (downloads JUCE and the ARA SDK)" ON)
option(AMT_BUILD_TESTS "Build the core unit tests" ON)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# Pitch-preserving time stretching (MIT licensed)
FetchContent_Declare(signalsmith-linear
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
    GIT_TAG 0.6.4
    GIT_SHALLOW ON)
FetchContent_Declare(signalsmith-stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG 1.4.0
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(signalsmith-linear signalsmith-stretch)

add_subdirectory(core)

if(AMT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(AMT_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()
