unset(CMAKE_CXX_FLAGS CACHE)
unset(CMAKE_C_FLAGS CACHE)
unset(CMAKE_SHARED_LINKER_FLAGS CACHE)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -g3 -ggdb -Wall -pipe -fpermissive -fexceptions -std=c++11 -fPIC -Wno-narrowing")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -g -O0 -g3 -ggdb -Wall -pipe -fexceptions -fPIC -Wno-narrowing -std=c99")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3  -std=c++11")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O3 -std=c99")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-export-dynamic -s")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${FFMPEG_INC_PATH})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ijkyuv/include)

set(ijksdl_SRC
  ijksdl_aout.c
  ijksdl_audio.c
  ijksdl_error.c
  ijksdl_mutex.c
  ijksdl_stdinc.c
  ijksdl_thread.c
  ijksdl_timer.c
  ijksdl_vout.c
  ijksdl_egl.c
  gles2/color.c
  gles2/common.c
  gles2/renderer.c
  gles2/renderer_rgb.c
  gles2/renderer_yuv420p.c
  gles2/renderer_yuv444p10le.c
  gles2/shader.c
  gles2/fsh/rgb.fsh.c
  gles2/fsh/yuv420p.fsh.c
  gles2/fsh/yuv444p10le.fsh.c
  gles2/vsh/mvp.vsh.c
  yuv2rgb/yuv2rgb16tab.c
  yuv2rgb/yuv420rgb565c.c
  yuv2rgb/yuv420rgb888c.c
  yuv2rgb/yuv420rgb8888c.c
  dummy/ijksdl_vout_dummy.c
  ffmpeg/ijksdl_vout_overlay_ffmpeg.c
  ffmpeg/abi_all/image_convert.c
  android/android_arraylist.c
  android/android_audiotrack.c
  android/android_build.c
  android/android_bundle.c
  android/android_bytebuffer.c
  android/android_nativewindow.c
  android/android_vout_gles.c
  android/ijksdl_android_jni.c
  android/ijksdl_aout_android_audiotrack.c
  android/ijksdl_aout_android_opensles.c
  android/ijksdl_codec_android_mediacodec_dummy.c
  android/ijksdl_codec_android_mediacodec_java.c
  android/ijksdl_codec_android_mediacodec.c
  android/ijksdl_codec_android_mediadef.c
  android/ijksdl_codec_android_mediaformat_java.c
  android/ijksdl_codec_android_mediaformat.c
  android/ijksdl_vout_android_nativewindow.c
  android/ijksdl_vout_android_surface.c
  android/ijksdl_vout_overlay_android_mediacodec.c
)

add_library(ijksdl SHARED ${ijksdl_SRC})

add_library( ijkffmpeg
             SHARED
             IMPORTED )
set_target_properties( ijkffmpeg
                       PROPERTIES IMPORTED_LOCATION
                       ${FFMPEG_LIB_PATH}/libijkffmpeg.so )

add_library(cpufeatures STATIC
            ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)

# Include libraries needed for hello-jni lib
target_link_libraries(ijksdl
                      ijkffmpeg
                      ijkutil
                      yuv_static
                      log
                      android
                      OpenSLES
                      GLESv2
                      EGL)
