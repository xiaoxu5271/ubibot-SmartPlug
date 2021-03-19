idf_component_register(SRCS ./My_Mqtt.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                       REQUIRES mqtt
                       PRIV_REQUIRES  Json_parse Smartconfig Bluetooth Led Http EC20)

