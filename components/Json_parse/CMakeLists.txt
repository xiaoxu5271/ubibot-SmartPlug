idf_component_register(SRCS ./Json_parse.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                    REQUIRES EC20 Smartconfig json E2prom Http Cache_data Bluetooth ota Led my_base64 RS485_Read ds18b20 RtcUsr CSE7759B Switch My_Mqtt )

