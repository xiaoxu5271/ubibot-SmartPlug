#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"

#define IO_UART_PIN 26
#define OI_RXD gpio_get_level(IO_UART_PIN)

#define TIMER_DIVIDER 16                             //  Hardware timer clock divider
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER) // convert counter value to seconds

uint8_t len = 0;
uint8_t check_flag = 0;
uint8_t recv_buff[24];

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

//IO中断函数
void IRAM_ATTR gpio_isr_handler(void *arg)
{
    if (OI_RXD == 0)
    {
        if (recvStat == COM_STOP_BIT)
        {
            recvStat = COM_START_BIT;
            timer_start(TIMER_GROUP_0, TIMER_1);
            // TIM_Cmd(TIM4, ENABLE);
            // esp_timer_start_periodic(fw_timer_handle, 208); //us
        }
    }
}

//硬件定时器中断函数
void IRAM_ATTR timer_group0_isr(void *para)
{
    int timer_idx = (int)para;
    timer_intr_t timer_intr = timer_group_intr_get_in_isr(TIMER_GROUP_0);

    if (timer_intr & TIMER_INTR_T1)
    {
        timer_group_intr_clr_in_isr(TIMER_GROUP_0, TIMER_1);
    }
    else
    {
        return;
    }
    // io_state = ~io_state;
    // gpio_set_level(GPIO_NUM_25, io_state);
    recvStat++;
    if (recvStat == COM_STOP_BIT)
    {
        timer_pause(TIMER_GROUP_0, TIMER_1);

        if (check_flag == 0 && (((recvData & 0xf0) == 0xf0) || (recvData == 0xaa) || (recvData == 0x55)))
        {
            check_flag = 1;
        }
        if (check_flag == 1)
        {
            recv_buff[len++] = recvData;
            if (len > 23)
            {
                check_flag = 0;
                len = 0;
                xQueueSendFromISR(Soft_uart_evt_queue, &recv_buff, NULL);
            }
        }

        timer_group_enable_alarm_in_isr(TIMER_GROUP_0, timer_idx);
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
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, timer_idx);
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

/****硬件定时器*****/
static void example_tg0_timer_init(int timer_idx,
                                   bool auto_reload, double timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = auto_reload;
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr,
                       (void *)timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    // timer_start(TIMER_GROUP_0, timer_idx);
}

void soft_uart_init(void)
{
    Soft_uart_evt_queue = xQueueCreate(1, sizeof(uint32_t));
    gpio_uart_init();
    example_tg0_timer_init(TIMER_1, 1, 0.000208);
}
