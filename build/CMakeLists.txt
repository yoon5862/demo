cmake_minimum_required(VERSION 3.16)
project(multi_png_sample C CXX)

set(TOP_DIR "..")
set(SRC_DIR "${TOP_DIR}/src")
set(OBJ_DIR "${TOP_DIR}/obj/")
set(BIN_DIR "${TOP_DIR}/bin/")


set(TENSORFLOW_SOURCE_DIR "" CACHE PATH
  "Directory that contains the TensorFlow project"
)

if(NOT TENSORFLOW_SOURCE_DIR)
  get_filename_component(TENSORFLOW_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../src/tensorflow_src"
    ABSOLUTE
  )
endif()

add_subdirectory(
  "${TENSORFLOW_SOURCE_DIR}/tensorflow/lite"
  "${CMAKE_CURRENT_BINARY_DIR}/tensorflow-lite"
  EXCLUDE_FROM_ALL
)

set(CMAKE_CXX_FLAGS_RELEASE "-O2")


# simple app

set(CMAKE_CXX_FLAGS " -marm -mfpu=neon -mfloat-abi=softfp -mcpu=cortex-a9 -mtune=cortex-a9 -rdynamic    -Wformat -Wformat-security -Werror=format-security -funwind-tables  --sysroot=/opt/starfish-sdk-x86_64/9.0.0/sysroots/ca9v1-starfishmllib32-linux-gnueabi  -O2 -pipe -g -feliminate-unused-debug-types  -fvisibility-inlines-hidden  -Wl,--allow-shlib-undefined")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2 -g --sysroot=/opt/starfish-sdk-x86_64/9.0.0/sysroots/ca9v1-starfishmllib32-linux-gnueabi -DLOG_ENABLE")

set(CMAKE_CXX_LINK_FLAGS " --sysroot=/opt/starfish-sdk-x86_64/9.0.0/sysroots/ca9v1-starfishmllib32-linux-gnueabi -lrt -ldl -lm -lstdc++ -lpng16")
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -L/home/lee/sample_app/multi_png_sample/lib -Wl,--rpath-link,/home/lee/sample_app/multi_png_sample/lib -lfreetype -lEGL -lGLESv2 -lm -lwayland-client -lwayland-egl -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_dnn -lwayland-webos-client -lpthread -Wl,--allow-shlib-undefined")

include_directories(
    ${SRC_DIR}
    ${TOP_DIR}/include
    ${TOP_DIR}/include/png
    ${TOP_DIR}/include/EGL
    ${TOP_DIR}/include/GLES2
)


set(SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/common.cpp
    ${SRC_DIR}/MultiplePNGIO.cpp
    ${SRC_DIR}/Renderer.cpp
    ${SRC_DIR}/quality_aware_scheduler.cpp
    ${SRC_DIR}/monitoring.cpp
    ${SRC_DIR}/util.cpp
)




set(CMAKE_CXX_STANDARD 11)

add_executable(multi_png_sample
${SOURCES}
)
target_link_libraries(multi_png_sample
tensorflow-lite
)
