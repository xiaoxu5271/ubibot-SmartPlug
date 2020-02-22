#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "Json_parse.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "Uart0.h"

#define UART0_TXD (UART_PIN_NO_CHANGE)
#define UART0_RXD (UART_PIN_NO_CHANGE)
#define UART0_RTS (UART_PIN_NO_CHANGE)
#define UART0_CTS (UART_PIN_NO_CHANGE)

#define UART2_TXD (GPIO_NUM_17)
#define UART2_RXD (GPIO_NUM_16)
#define UART2_RTS (UART_PIN_NO_CHANGE)
#define UART2_CTS (UART_PIN_NO_CHANGE)

#define BUF_SIZE 256
static const char *TAG = "UART0";
SemaphoreHandle_t xMutex_uart2_sw = NULL;

static void Uart0_Task(void *arg);

void Uart_Init(void)
{
    //uart0,log
    uart_config_t uart0_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    uart_param_config(UART_NUM_0, &uart0_config);
    uart_set_pin(UART_NUM_0, UART0_TXD, UART0_RXD, UART0_RTS, UART0_CTS);
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);

    //uart2,485
    uart_config_t uart2_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    uart_param_config(UART_NUM_2, &uart2_config);
    uart_set_pin(UART_NUM_2, UART2_TXD, UART2_RXD, UART2_RTS, UART2_CTS);
    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0);

    //uart2 switch io
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1 << UART2_SW);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //创建切换uart2 互斥信号
    xMutex_uart2_sw = xSemaphoreCreateMutex();
    //串口0 接收解析
    xTaskCreate(Uart0_Task, "Uart0_Task", 4096, NULL, 9, NULL);
}

void sw_uart2(uint8_t uart2_mode)
{
    //
    xSemaphoreTake(xMutex_uart2_sw, portMAX_DELAY);
    if (uart2_mode == uart2_485)
    {
        gpio_set_level(UART2_SW, 0); //RS485输出模式
        uart_flush(UART_NUM_2);
        uart_flush_input(UART_NUM_2);
        uart_set_baudrate(UART_NUM_2, 9600);

        vTaskDelay(10 / portTICK_RATE_MS);
    }
    else if (uart2_mode == uart2_cse)
    {
        gpio_set_level(UART2_SW, 1); //电能输出模式
        uart_flush(UART_NUM_2);
        uart_flush_input(UART_NUM_2);
        uart_set_baudrate(UART_NUM_2, 4800);

        vTaskDelay(10 / portTICK_RATE_MS);
    }
}

void Uart0_read(void)
{
    uint8_t data_u0[BUF_SIZE];

    int len0 = uart_read_bytes(UART_NUM_0, data_u0, BUF_SIZE, 20 / portTICK_RATE_MS);
    if (len0 != 0) //读取到按键数据
    {
        len0 = 0;
        ESP_LOGW(TAG, "data_u0=%s", data_u0);
        ParseTcpUartCmd((char *)data_u0);
        bzero(data_u0, sizeof(data_u0));
    }
}

void Uart0_Task(void *arg)
{
    while (1)
    {
        Uart0_read();
        vTaskDelay(10 / portTICK_RATE_MS);
    }
}
