add_library( ijkffmpeg
             SHARED
             IMPORTED )
set_target_properties( ijkffmpeg
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_LIB_PATH}/libijkffmpeg.so )

file(COPY "${FFMPEG_LIB_PATH}/libijkffmpeg.so" DESTINATION "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}" NO_SOURCE_PERMISSIONS)