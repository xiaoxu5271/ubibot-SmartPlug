#ifndef __NV_S
#define __NV_S
#include "esp_err.h"
void nvs_write(char *write_nvs, char *write_data);
esp_err_t nvs_read(char *read_nvs);
#endif