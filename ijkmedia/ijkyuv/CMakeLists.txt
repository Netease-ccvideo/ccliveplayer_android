unset(CMAKE_CXX_FLAGS CACHE)
unset(CMAKE_C_FLAGS CACHE)
unset(CMAKE_SHARED_LINKER_FLAGS CACHE)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -g3 -ggdb -Wall -pipe -fpermissive -fexceptions -std=c++11 -fPIC -Wno-narrowing")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -g -O0 -g3 -ggdb -Wall -pipe -fexceptions -fPIC -Wno-narrowing -std=c99")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -pipe -fpermissive -fexceptions -std=c++11 -fPIC -Wno-narrowing -frtti -ffunction-sections -fdata-sections -fvisibility=hidden")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O3 -Wall -pipe -fexceptions -fPIC -Wno-narrowing -frtti -ffunction-sections -fdata-sections -fvisibility=hidden -std=c99")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-export-dynamic -s")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(libyuv_static_SRC
        source/compare.cc
        source/compare_common.cc
        source/compare_posix.cc
        source/convert.cc
        source/convert_argb.cc
        source/convert_from.cc
        source/convert_from_argb.cc
        source/convert_to_argb.cc
        source/convert_to_i420.cc
        source/cpu_id.cc
        source/format_conversion.cc
        source/planar_functions.cc
        source/rotate.cc
        source/rotate_argb.cc
        source/rotate_mips.cc
        source/row_any.cc
        source/row_common.cc
        source/row_mips.cc
        source/row_posix.cc
        source/scale.cc
        source/scale_argb.cc
        source/scale_common.cc
        source/scale_mips.cc
        source/scale_posix.cc
        source/video_common.cc
        )

if(${ANDROID_ABI} STREQUAL "armeabi-v7a")
    set(libyuv_static_SRC
            ${libyuv_static_SRC}
            source/compare_neon.cc
            source/rotate_neon.cc
            source/row_neon.cc
            source/scale_neon.cc
            )
    add_definitions(-DLIBYUV_NEON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLIBYUV_NEON -mfloat-abi=softfp -mfpu=neon")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -DLIBYUV_NEON -mfloat-abi=softfp -mfpu=neon")
elseif(${ANDROID_ABI} STREQUAL "arm64-v8a")
    set(libyuv_static_SRC
            ${libyuv_static_SRC}
            source/compare_neon64.cc
            source/rotate_neon64.cc
            source/row_neon64.cc
            source/scale_neon64.cc
            )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLIBYUV_NEON -mfloat-abi=softfp -mfpu=neon")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -DLIBYUV_NEON -mfloat-abi=softfp -mfpu=neon")
else()
    set(libyuv_static_SRC
            ${libyuv_static_SRC}
            source/compare_neon.cc
            source/rotate_neon.cc
            source/row_neon.cc
            source/scale_neon.cc
            )
endif()

add_library(yuv_static STATIC ${libyuv_static_SRC})


