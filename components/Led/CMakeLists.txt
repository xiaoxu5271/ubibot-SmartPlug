idf_component_register(SRCS ./Led.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                       PRIV_REQUIRES Smartconfig Json_parse)

