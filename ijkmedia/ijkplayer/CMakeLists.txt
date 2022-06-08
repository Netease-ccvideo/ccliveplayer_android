unset(CMAKE_CXX_FLAGS CACHE)
unset(CMAKE_C_FLAGS CACHE)
unset(CMAKE_SHARED_LINKER_FLAGS CACHE)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -g3 -ggdb -Wall -pipe -fpermissive -fexceptions -std=c++11 -fPIC -Wno-narrowing")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -g -O0 -g3 -ggdb -Wall -pipe -fexceptions -fPIC -Wno-narrowing -mfloat-abi=soft -std=c99")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -pipe -fpermissive -fexceptions -std=c++11 -fPIC -Wno-narrowing -frtti -ffunction-sections -fdata-sections -fvisibility=hidden")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O3 -Wall -pipe -fexceptions -fPIC -Wno-narrowing -frtti -ffunction-sections -fdata-sections -fvisibility=hidden -mfloat-abi=soft -std=c99")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-export-dynamic -s")
endif()

set(STAT_PATH         ${CMAKE_CURRENT_SOURCE_DIR}/stat)
set(IJK4A_PATH        ${CMAKE_CURRENT_SOURCE_DIR}/ijk4a)
set(IJKAVFORMAT       ${CMAKE_CURRENT_SOURCE_DIR}/ijkavformat)
set(OPENSSL_LIB_PATH  ${CMAKE_SOURCE_DIR}/ffmpeg/include/openssl)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${FFMPEG_INC_PATH})
include_directories(${CMAKE_SOURCE_DIR}/ffmpeg/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ijkyuv/include)
include_directories(${STAT_PATH})
include_directories(${OPENSSL_LIB_PATH})


set(ijkplayer_SRC
  ff_cmdutils.c
  ff_ffplay.c
  ff_file_saver.c
  ff_ffpipeline.c
  ff_ffpipenode.c
  ijkmeta.c
  ijkplayer.c
  pipeline/ffpipeline_ffplay.c
  pipeline/ffpipenode_ffplay_vdec.c
  pipeline/ffpipenode_ffplay_vout.c
  android/ffmpeg_api_jni.c
  android/ijkplayer_android.c
  android/ijkplayer_jni.c
  android/pipeline/ffpipeline_android.c
  android/pipeline/ffpipenode_android_mediacodec_vdec.c
  android/pipeline/ffpipenode_android_mediacodec_vout.c
  ${IJK4A_PATH}/j4a_base.c
  ${IJK4A_PATH}/IMediaDataSource.c
  ${IJKAVFORMAT}/ijkmediadatasource.c
  ${IJKAVFORMAT}/allformats.c
)

add_library(ijkplayer SHARED ${ijkplayer_SRC})

add_library( ijkffmpeg
             SHARED
             IMPORTED)
set_target_properties( ijkffmpeg
                       PROPERTIES IMPORTED_LOCATION
                       ${FFMPEG_LIB_PATH}/libijkffmpeg.so )

# Include libraries needed for hello-jni lib
target_link_libraries(ijkplayer
                      ijkffmpeg
                      ijkutil
                      ijksdl
                      ijksoundtouch
                      log
                      android)