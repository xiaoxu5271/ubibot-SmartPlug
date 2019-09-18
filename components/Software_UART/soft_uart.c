#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

#define IO_UART_PIN 26
#define OI_RXD gpio_get_level(IO_UART_PIN)

xQueueHandle Soft_uart_evt_queue;
//定时器句柄
esp_timer_handle_t fw_timer_handle = 0;
void IRAM_ATTR gpio_isr_handler(void *arg);
void fw_timer_cb(void);

enum
{
    COM_START_BIT,
    COM_D0_BIT,
    COM_D1_BIT,
    COM_D2_BIT,
    COM_D3_BIT,
    COM_D4_BIT,
    COM_D5_BIT,
    COM_D6_BIT,
    COM_D7_BIT,
    COM_STOP_BIT,
};
uint8_t recvStat = COM_STOP_BIT;
uint8_t recvData = 0;

void en_uart_recv(void)
{
    gpio_set_intr_type(IO_UART_PIN, GPIO_INTR_NEGEDGE);
}
void dis_uart_recv(void)
{
    gpio_set_intr_type(IO_UART_PIN, GPIO_INTR_DISABLE);
}

void gpio_uart_init(void)
{
    //配置GPIO，下降沿触发中断
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = 1 << IO_UART_PIN;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //初始状态 禁止中断
    gpio_set_intr_type(IO_UART_PIN, GPIO_INTR_DISABLE);

    //注册中断服务
    gpio_install_isr_service(0);
    gpio_isr_handler_add(IO_UART_PIN, gpio_isr_handler, (void *)IO_UART_PIN);
}

void time_uart_init()
{
    //定时器结构体初始化
    esp_timer_create_args_t fw_timer =
        {
            .callback = &fw_timer_cb, //回调函数
            .arg = NULL,              //参数
            .name = "fw_timer"        //定时器名称
        };

    //定时器创建、启动
    esp_timer_create(&fw_timer, &fw_timer_handle);
    // err = esp_timer_start_periodic(fw_timer_handle, 1000 * 1000); //1秒回调
    // if (err == ESP_OK)
    // {
    //     printf("fw timer cteate and start ok!\r\n");
    // }
}

void soft_uart_init(void)
{
    Soft_uart_evt_queue = xQueueCreate(1, sizeof(uint32_t));
    gpio_uart_init();
    time_uart_init();
}

void IRAM_ATTR gpio_isr_handler(void *arg)
{
    if (OI_RXD == 0)
    {
        if (recvStat == COM_STOP_BIT)
        {
            recvStat = COM_START_BIT;
            // TIM_Cmd(TIM4, ENABLE);
            esp_timer_start_periodic(fw_timer_handle, 208); //us
        }
    }
}

void fw_timer_cb(void)
{
    recvStat++;
    if (recvStat == COM_STOP_BIT)
    {
        // TIM_Cmd(TIM4, DISABLE);
        esp_timer_stop(fw_timer_handle);
        // ESP_LOGI("soft_uart", "%2x", recvData);
        xQueueSendFromISR(Soft_uart_evt_queue, &recvData, NULL);
        return;
    }
    if (OI_RXD)
    {
        recvData |= (1 << (recvStat - 1));
    }
    else
    {
        recvData &= ~(1 << (recvStat - 1));
    }
}
