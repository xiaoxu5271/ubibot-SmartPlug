idf_component_register(SRCS ./Switch.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                       PRIV_REQUIRES json Cache_data ServerTimer Http Json_parse Led Bluetooth CSE7759B E2prom Smartconfig)

