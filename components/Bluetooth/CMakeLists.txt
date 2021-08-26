idf_component_register(SRCS ./Bluetooth.c
                       INCLUDE_DIRS include                       # Edit following two lines to set component requirements (see docs)
                       REQUIRES bt Json_parse Smartconfig E2prom Http Led ServerTimer)

