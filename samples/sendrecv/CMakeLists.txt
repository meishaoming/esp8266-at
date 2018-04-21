set(target esp8266-pc)
add_executable(${target} main.c ../common/serial.c)
target_link_libraries(${target} esp8266 atcmd ringbuffer)
