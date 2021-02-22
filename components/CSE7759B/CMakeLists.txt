idf_component_register(SRCS ./CSE7759B.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                       PRIV_REQUIRES json Json_parse Uart0 Http Cache_data ServerTimer Led Smartconfig)

