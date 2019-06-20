#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "Json_parse.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "Uart0.h"



#define UART0_TXD  (UART_PIN_NO_CHANGE)
#define UART0_RXD  (UART_PIN_NO_CHANGE)
#define UART0_RTS  (UART_PIN_NO_CHANGE)
#define UART0_CTS  (UART_PIN_NO_CHANGE)

#define BUF_SIZE    256
static const char *TAG = "UART0";


void Uart0_Init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, UART0_TXD, UART0_RXD, UART0_RTS, UART0_CTS);
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);

}


void Uart0_read(void)
{
    uint8_t data_u0[BUF_SIZE];
 
    int len0 = uart_read_bytes(UART_NUM_0, data_u0, BUF_SIZE, 20 / portTICK_RATE_MS);
    if(len0!=0)  //读取到按键数据
    {
        len0=0;
        ESP_LOGW(TAG, "data_u0=%s",data_u0);
        parse_Uart0((char *)data_u0);
        bzero(data_u0,sizeof(data_u0));
    }
     
}




