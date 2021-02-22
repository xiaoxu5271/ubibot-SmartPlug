idf_component_register(SRCS ./ds18b20.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                       PRIV_REQUIRES json Http ServerTimer Cache_data Json_parse Smartconfig ds18b20 )

