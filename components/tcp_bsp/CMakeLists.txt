idf_component_register(SRCS ./tcp_bsp.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                       PRIV_REQUIRES tcp_bsp Smartconfig Json_parse)

