idf_component_register(SRCS ./user_app.c ./user_key.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                    #    REQUIRES )
                       PRIV_REQUIRES Smartconfig Mqtt ota Bluetooth E2prom Json_parse Switch Http Led Cache_data)

