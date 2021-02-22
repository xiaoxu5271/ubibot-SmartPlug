idf_component_register(SRCS ./E2prom.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                       PRIV_REQUIRES  Json_parse Http user_keydriver Led crc8_16 ota)

