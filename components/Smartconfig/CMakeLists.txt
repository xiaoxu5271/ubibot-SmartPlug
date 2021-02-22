idf_component_register(SRCS ./Smartconfig.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                       PRIV_REQUIRES Led Json_parse Bluetooth EC20 Mqtt Http)

