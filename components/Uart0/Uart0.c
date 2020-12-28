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

#define BUF_SIZE 1024
static const char *TAG = "UART0";
SemaphoreHandle_t xMutex_uart2_sw = NULL;
QueueHandle_t Log_uart_queue;

static void Uart0_Task(void *arg);

void Uart_Init(void)
{
    //uart0,log
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    //Install UART driver, and get the queue.
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 2, &Log_uart_queue, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, UART0_TXD, UART0_TXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

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
    xTaskCreate(Uart0_Task, "Uart0_Task", 5120, NULL, 9, NULL);
}

void sw_uart2(uint8_t uart2_mode)
{
    //
    xSemaphoreTake(xMutex_uart2_sw, portMAX_DELAY);
    switch (uart2_mode)
    {
    case uart2_485:
        gpio_set_level(UART2_SW, 0); //RS485输出模式
        uart_flush(UART_NUM_2);
        uart_flush_input(UART_NUM_2);
        uart_set_baudrate(UART_NUM_2, 9600);
        break;

    case uart2_cse:
        gpio_set_level(UART2_SW, 1); //电能输出模式
        uart_flush(UART_NUM_2);
        uart_flush_input(UART_NUM_2);
        uart_set_baudrate(UART_NUM_2, 4800);
        break;

    case uart2_co2:
        gpio_set_level(UART2_SW, 0); //CO2输出模式
        uart_flush(UART_NUM_2);
        uart_flush_input(UART_NUM_2);
        uart_set_baudrate(UART_NUM_2, 19200);

    default:
        break;
    }
    vTaskDelay(10 / portTICK_RATE_MS);
}

void Uart0_Task(void *arg)
{
    uart_event_t event;
    uint8_t data_u0[BUF_SIZE] = {0};
    uint16_t all_read_len = 0;
    while (1)
    {
        if (xQueueReceive(Log_uart_queue, (void *)&event, (portTickType)portMAX_DELAY))
        {
            switch (event.type)
            {
            case UART_DATA:
                if (event.size >= 1)
                {
                    if (all_read_len + event.size >= BUF_SIZE)
                    {
                        ESP_LOGE(TAG, "read len flow");
                        all_read_len = 0;
                        memset(data_u0, 0, BUF_SIZE);
                    }
                    uart_read_bytes(UART_NUM_0, data_u0 + all_read_len, event.size, portMAX_DELAY);

                    uxQueueMessagesWaiting(Log_uart_queue);

                    all_read_len += event.size;
                    data_u0[all_read_len] = 0; //去掉字符串结束符，防止字符串拼接不成功

                    if ((data_u0[all_read_len - 1] == '\r') || (data_u0[all_read_len - 1] == '\n'))
                    {
                        ESP_LOGI(TAG, "uart0 recv,  len:%d,%s", strlen((const char *)data_u0), data_u0);
                        ParseTcpUartCmd((char *)data_u0);
                        all_read_len = 0;
                        memset(data_u0, 0, BUF_SIZE);
                        uart_flush(UART_NUM_0);
                    }
                }
                break;

            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                uart_flush_input(UART_NUM_0);
                xQueueReset(Log_uart_queue);
                break;

            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                uart_flush_input(UART_NUM_0);
                xQueueReset(Log_uart_queue);
                break;

            //Others
            default:
                // ESP_LOGI(TAG, "uart type: %d", event.type);
                break;
            }
        }
    }
}
