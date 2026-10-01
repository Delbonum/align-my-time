add_library(amt_core STATIC
    src/Alignment.cpp
    src/AudioClip.cpp
    src/Markers.cpp
    src/OnsetDetector.cpp
    src/Renderers.cpp
    src/TempoMap.cpp
    src/WarpMap.cpp)

target_include_directories(amt_core PUBLIC include)
target_link_libraries(amt_core PRIVATE signalsmith-stretch)

if(MSVC)
    target_compile_options(amt_core PRIVATE /W4 /bigobj)
else()
    target_compile_options(amt_core PRIVATE -Wall -Wextra)
endif()
