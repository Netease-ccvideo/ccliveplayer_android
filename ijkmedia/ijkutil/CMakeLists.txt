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

set(ijkutil_SRC
  android/loghelp.c
  android/jnihelp.c
)

add_library(ijkutil SHARED ${ijkutil_SRC})

# Include libraries needed for hello-jni lib
target_link_libraries(ijkutil
                      log)
