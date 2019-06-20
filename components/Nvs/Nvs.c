
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

void nvs_write(char *write_nvs, char *write_data)
{
    esp_err_t err;
    // Open
    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle...%s ", write_data);
    nvs_handle my_handle;
  
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }

    err = nvs_set_i32(my_handle, write_nvs, write_data); // Write
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

    nvs_commit(my_handle);

    nvs_close(my_handle);
}
esp_err_t nvs_read(char *read_nvs)
{
    esp_err_t err;
    char *write_nvs = NULL;
    // Open
    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle my_handle;

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {

        err = nvs_get_i32(my_handle, read_nvs, &write_nvs);
        switch (err)
        {
        case ESP_OK:
            printf("this is read nvs %s\n", write_nvs);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf("The value is not initialized yet!\n");
            break;
        default:
            printf("Error (%s) reading!\n", esp_err_to_name(err));
        }

        nvs_commit(my_handle);

        nvs_close(my_handle);
    }
    return write_nvs;
}
