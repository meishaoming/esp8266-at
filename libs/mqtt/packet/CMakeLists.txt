file(GLOB SOURCES "*.c")
add_library(paho-embed-mqtt3c ${SOURCES})
target_compile_definitions(paho-embed-mqtt3c PRIVATE MQTT_SERVER MQTT_CLIENT)

add_library(MQTTPacketClient MQTTFormat MQTTPacket
            MQTTSerializePublish MQTTDeserializePublish
            MQTTConnectClient MQTTSubscribeClient MQTTUnsubscribeClient)
target_compile_definitions(MQTTPacketClient PRIVATE MQTT_CLIENT)

add_library(MQTTPacketServer MQTTFormat MQTTPacket
            MQTTSerializePublish MQTTDeserializePublish
            MQTTConnectServer MQTTSubscribeServer MQTTUnsubscribeServer)
target_compile_definitions(MQTTPacketServer PRIVATE MQTT_SERVER)
