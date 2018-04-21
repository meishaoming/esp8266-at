file(GLOB SOURCES "*.c" "nonos/*.c")

add_library(paho-embed-mqtt3cc ${SOURCES})

target_include_directories(paho-embed-mqtt3cc PRIVATE "nonos")

target_link_libraries(paho-embed-mqtt3cc paho-embed-mqtt3c)

target_compile_definitions(paho-embed-mqtt3cc PRIVATE MQTTCLIENT_PLATFORM_HEADER=mqtt-nonos.h MQTTCLIENT_QOS2=1)
