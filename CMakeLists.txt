cmake_minimum_required(VERSION 3.16)

project(ffplay VERSION 1.0 LANGUAGES C CXX)

option(DEBUG "With debug" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(DEBUG)
    set(DEBUG_DEFINE "-g -O0 -fno-omit-frame-pointer")
else()
    set(DEBUG_DEFINE "-O2")
endif()

set(CMAKE_CXX_FLAGS "${DEBUG_DEFINE} -pipe -Wall -W -Wno-unused-parameter -fstrict-aliasing -Wno-invalid-offsetof -march=native")
set(CMAKE_C_FLAGS "${DEBUG_DEFINE} -pipe -Wall -fstrict-aliasing -march=native")

file(GLOB_RECURSE sources *.cpp)
add_executable(filebroadcaster ${sources} main.cpp clock.c queue.c)
target_link_directories(filebroadcaster PRIVATE /usr/local/lib)
target_include_directories(filebroadcaster PRIVATE /usr/local/include/)
target_link_libraries(filebroadcaster PRIVATE libwebrtc swresample avutil avresample avformat avcodec pthread)

#file(GLOB_RECURSE ffplay_sources *.c)
#add_executable(ffplay ${ffplay_sources})
#target_link_directories(ffplay PRIVATE /usr/local/lib)
#target_include_directories(ffplay PRIVATE /usr/local/include/ /usr/local/include/SDL2)
#target_link_libraries(ffplay PRIVATE SDL2 swresample avutil avresample avformat avcodec pthread)

