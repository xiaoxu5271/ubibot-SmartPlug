idf_component_register(SRCS ./Cache_data.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                       PRIV_REQUIRES json Http Json_parse Led Bluetooth CSE7759B Spi_flash E2prom Smartconfig ServerTimer Mqtt)

