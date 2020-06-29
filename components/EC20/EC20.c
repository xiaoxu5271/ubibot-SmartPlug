#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include <cJSON.h>

#include "driver/uart.h"
#include "soc/uart_periph.h"
#include "driver/gpio.h"
#include "Smartconfig.h"
#include "Led.h"
#include "Json_parse.h"
#include "Http.h"
#include "Switch.h"

#include "EC20.h"

#define UART1_TXD (GPIO_NUM_21)
#define UART1_RXD (GPIO_NUM_22)

static const char *TAG = "EC20";
TaskHandle_t EC20_Task_Handle;
TaskHandle_t EC20_M_Task_Handle;
TaskHandle_t Uart1_Task_Handle;
TaskHandle_t EC20_Mqtt_Handle;

char *mqtt_recv;
char *EC20_RECV;

#define EC20_SW 25
#define BUF_SIZE 1024

#define EX_UART_NUM UART_NUM_1

QueueHandle_t EC_uart_queue;
QueueHandle_t EC_at_queue;
QueueHandle_t EC_ota_queue;
QueueHandle_t EC_mqtt_queue;

char ICCID[24] = {0};
uint8_t EC_RECV_MODE = EC_NORMAL;

uint16_t file_len = 0;

extern char topic_s[100];
extern char topic_p[100];
extern bool MQTT_E_STA;

char Wallkey_Check[] = {0X7E, 0X08, 0X0D, 0X59, 0X09, 0X02, 0X00, 0X02, 0XEF, 0X07, 0X0D};

void uart_event_task(void *pvParameters);

void uart_event_task(void *pvParameters)
{
    xEventGroupSetBits(Net_sta_group, Uart1_Task_BIT);
    uart_event_t event;
    uint16_t all_read_len = 0;
    EC20_RECV = (char *)malloc(BUF_SIZE);
    bool check_sta = false;

    for (;;)
    {
        //Waiting for UART event.
        if (xQueueReceive(EC_uart_queue, (void *)&event, (portTickType)portMAX_DELAY))
        {
            switch (event.type)
            {
            case UART_DATA:
                // ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                if (event.size > 1)
                {
                    if (all_read_len + event.size >= BUF_SIZE)
                    {
                        ESP_LOGE(TAG, "read len flow");
                        uart_flush(EX_UART_NUM);
                        all_read_len = 0;
                        memset(EC20_RECV, 0, BUF_SIZE);
                        xQueueReset(EC_uart_queue);
                        continue; //此处需返回循环，否则会导致循环错误
                    }
                    uart_read_bytes(EX_UART_NUM, (uint8_t *)EC20_RECV, event.size, portMAX_DELAY);
                    esp_log_buffer_hex(TAG, EC20_RECV, event.size);
                    if (event.size == 11)
                    {
                        for (uint8_t i = 0; i < 11; i++)
                        {
                            if (EC20_RECV[i] != Wallkey_Check[i])
                            {
                                check_sta = false;
                                break;
                            }
                            else
                            {
                                check_sta = true;
                            }
                        }
                        if (check_sta == true)
                        {
                            Switch_Relay(-1);
                        }
                    }

                    all_read_len = 0;
                    memset(EC20_RECV, 0, BUF_SIZE);
                }

                break;

            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                uart_flush_input(EX_UART_NUM);
                xQueueReset(EC_uart_queue);
                break;

            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                uart_flush_input(EX_UART_NUM);
                xQueueReset(EC_uart_queue);
                break;

            //Others
            default:
                ESP_LOGI(TAG, "uart type: %d", event.type);
                break;
            }
        }
    }
    vTaskDelete(NULL);
}

void EC20_Init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_RTS,
        .source_clk = UART_SCLK_APB,
    };
    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, 0, 2, &EC_uart_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);
    uart_set_pin(EX_UART_NUM, UART1_TXD, UART1_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // //uart2 switch io
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1 << EC20_SW);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    if ((xEventGroupGetBits(Net_sta_group) & Uart1_Task_BIT) != Uart1_Task_BIT)
    {
        xTaskCreate(uart_event_task, "uart_event_task", 4096, NULL, 20, &Uart1_Task_Handle);
    }
}
