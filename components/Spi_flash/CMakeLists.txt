idf_component_register(SRCS ./my_spi_flash.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                       PRIV_REQUIRES json Cache_data Led)

