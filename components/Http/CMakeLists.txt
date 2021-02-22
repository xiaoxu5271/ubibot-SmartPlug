idf_component_register(SRCS ./Http.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                       PRIV_REQUIRES EC20 Json_parse E2prom Bluetooth Led Smartconfig my_base64 Http Mqtt )

