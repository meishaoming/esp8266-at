set(target esp8266-mqtt)
add_executable(${target} main.c ../common/serial.c)
target_link_libraries(${target} esp8266 atcmd ringbuffer)
target_link_libraries(${target} paho-embed-mqtt3cc paho-embed-mqtt3c)

target_include_directories(${target} PRIVATE
	"../../libs/mqtt/client/nonos"
	)
target_compile_definitions(${target} PRIVATE MQTTCLIENT_PLATFORM_HEADER=mqtt-nonos.h)
