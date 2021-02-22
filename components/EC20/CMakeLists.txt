idf_component_register(SRCS ./EC20.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                       PRIV_REQUIRES json Smartconfig Led Json_parse Http)

