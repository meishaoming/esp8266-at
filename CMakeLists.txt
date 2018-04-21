cmake_minimum_required(VERSION 3.10)

project(esp8266)

set(CMAKE_C_FLAGS "-Wall -Wextra")
include_directories(
	esp8266
	libs/atcmd
	libs/ringbuffer
	libs/mqtt/client
	libs/mqtt/packet
)

add_subdirectory(libs)
add_subdirectory(esp8266)
add_subdirectory(samples)
