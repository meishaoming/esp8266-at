cmake_minimum_required(VERSION 3.10)

project(esp8266)

set(target esp8266-pc)

set(sources main.c serial.c)

set(CMAKE_C_FLAGS "-Wall -Wextra")
include_directories(esp8266 libs/atcmd libs/ringbuffer)

add_subdirectory(libs)
add_subdirectory(esp8266)

add_executable(${target} ${sources})

target_link_libraries(${target} esp8266 atcmd ringbuffer)
