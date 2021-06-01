cmake_minimum_required(VERSION 3.16)

project(ffplay VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

if(DEBUG)
    set(DEBUG_DEFINE "-g -O0 -fno-omit-frame-pointer")
else()
    set(DEBUG_DEFINE "-O2")
endif()

set(CMAKE_CXX_FLAGS "${DEBUG_DEFINE} -pipe -Wall -W -Wno-unused-parameter -fstrict-aliasing -Wno-invalid-offsetof -march=native")
set(CMAKE_C_FLAGS "${DEBUG_DEFINE} -pipe -Wall -fstrict-aliasing -march=native")

file(GLOB_RECURSE sources *.c)

add_executable(ffplay ${sources})

target_include_directories(ffplay PRIVATE
        /usr/local/include/
        /usr/local/include/SDL2
        )
target_link_directories(ffplay PRIVATE  /usr/local/lib)

target_link_libraries(ffplay PRIVATE SDL2 swscale swresample postproc avutil avresample avformat avfilter avdevice avcodec)
