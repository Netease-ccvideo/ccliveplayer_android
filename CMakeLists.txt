cmake_minimum_required(VERSION 3.4.1)

SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../libs/${ANDROID_ABI})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../libs/${ANDROID_ABI})

set(FFMPEG_INC_PATH      ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/include)
set(FFMPEG_LIB_PATH      ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI})
#set(IJKMEDIA_PATH        ${CMAKE_SOURCE_DIR}/../../../ijkmedia)
set(IJKMEDIA_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/../build/output/ijkmedia)

include_directories(${CMAKE_SOURCE_DIR})

add_subdirectory(ffmpeg)
add_subdirectory(ijkmedia)