idf_component_register(SRCS ./RS485_Read.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                       PRIV_REQUIRES json Uart0 Http Cache_data ServerTimer Json_parse crc8_16 Smartconfig)

