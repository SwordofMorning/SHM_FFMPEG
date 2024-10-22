FILE(
    GLOB_RECURSE SRC_LIST 
    ./*.c
    ./*.cpp
)

# Exe output path
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

ADD_EXECUTABLE(${APP_NAME} ${SRC_LIST})

# Link lib and so
TARGET_LINK_LIBRARIES(
    ${APP_NAME}
    # ffmpeg
    libavformat.so;
    libswscale.so;
    libavcodec.so;
    libavutil.so;
    libavdevice.so;
    libavdevice.so;
    libswresample.so;
)