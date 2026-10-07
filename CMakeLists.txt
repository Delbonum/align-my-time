cmake_minimum_required(VERSION 3.24)

# Semantic versioning: MAJOR.MINOR.PATCH, bumped with every change (see CHANGELOG.md)
project(AlignMyTime VERSION 1.4.0 LANGUAGES C CXX)

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

    # Every UI text must have an English translation
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND AND AMT_BUILD_PLUGIN)
        add_test(NAME translations COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_translations.py)
    endif()
endif()

if(AMT_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()
