unset(CMAKE_CXX_FLAGS CACHE)
unset(CMAKE_C_FLAGS CACHE)
unset(CMAKE_SHARED_LINKER_FLAGS CACHE)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -g3 -ggdb -Wall -pipe -fpermissive -fexceptions -std=c++11 -fPIC -Wno-narrowing")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -g -O0 -g3 -ggdb -Wall -pipe -fexceptions -fPIC -Wno-narrowing -std=c99")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O3 -std=c99")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-export-dynamic  -s")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/)

set(ijksoundtouch_SRC
        source/SoundTouch/AAFilter.cpp
        source/SoundTouch/FIFOSampleBuffer.cpp
        source/SoundTouch/FIRFilter.cpp
        source/SoundTouch/cpu_detect_x86.cpp
        source/SoundTouch/sse_optimized.cpp
        source/SoundTouch/RateTransposer.cpp
        source/SoundTouch/InterpolateCubic.cpp
        source/SoundTouch/InterpolateLinear.cpp
        source/SoundTouch/InterpolateShannon.cpp
        source/SoundTouch/TDStretch.cpp
        source/SoundTouch/BPMDetect.cpp
        source/SoundTouch/PeakFinder.cpp
        source/SoundTouch/SoundTouch.cpp
        source/SoundTouch/mmx_optimized.cpp
        ijksoundtouch_wrap.cpp
)

add_library(ijksoundtouch STATIC ${ijksoundtouch_SRC})

# Include libraries needed for hello-jni lib
target_link_libraries(ijksoundtouch
                      log
                      android)
