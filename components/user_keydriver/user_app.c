
// ===========================
// 头文件包含
// ===========================
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "user_key.h"
#include <esp_log.h>
// #include "tcp_bsp.h"
#include "Smartconfig.h"
#include "Mqtt.h"
#include "ota.h"
#include "Switch.h"
#include "RtcUsr.h"

/* 创建用户按键执行消息列队 */
static xQueueHandle xQueue_user_key_app = NULL;

/* 填充需要配置的按键个数以及对应的相关参数 */
static key_config_t gs_m_key_config[BOARD_BUTTON_COUNT] =
    {
        {BOARD_BUTTON, APP_KEY_ACTIVE_LOW, 0, LONG_PRESSED_TIMER},
};

void vTask_view_Work(void)
{
    uint8_t pcWriteBuffer[2048];

    printf("free Heap:%d\n", esp_get_free_heap_size());
    // /* K1键按下 打印任务执行情况 */

    printf("=======================================================\r\n");
    printf("任务名           任务状态   优先级      剩余栈   任务序号\r\n");
    vTaskList((char *)&pcWriteBuffer);
    printf("%s\r\n", pcWriteBuffer);

    printf("\r\n任务名            运行计数              使用率\r\n");
    vTaskGetRunTimeStats((char *)&pcWriteBuffer);
    printf("%s\r\n", pcWriteBuffer);
}

/** 
 * 用户的短按处理函数
 * @param[in]   key_num                 :短按按键对应GPIO口
 * @param[in]   short_pressed_counts    :短按按键对应GPIO口按下的次数,这里用不上
 * @retval      null
 * @par         修改日志 
 *               Ver0.0.1:
                     Helon_Chan, 2018/06/16, 初始化版本\n 
 */
void short_pressed_cb(uint8_t key_num, uint8_t *short_pressed_counts)
{
    uint8_t Task_key_num = 0;
    switch (key_num)
    {
    case BOARD_BUTTON:
        switch (*short_pressed_counts)
        {
        case 1:
            // ESP_LOGI("short_pressed_cb", "first press!!!\n");
            Task_key_num = 1;
            break;

        case 2:
            // ESP_LOGI("short_pressed_cb", "double press!!!\n");
            Task_key_num = 2;
            break;

        case 3:
            // ESP_LOGI("short_pressed_cb", "trible press!!!\n");
            Task_key_num = 3;
            break;

        case 4:
            // ESP_LOGI("short_pressed_cb", "quatary press!!!\n");
            Task_key_num = 4;
            break;

        default:
            break;
        }
        *short_pressed_counts = 0;
        break;

    default:
        break;
    }

    xQueueSend(xQueue_user_key_app, (void *)&Task_key_num, (TickType_t)0);
}

/** 
 * 用户的长按处理函数
 * @param[in]   key_num                 :短按按键对应GPIO口
 * @param[in]   long_pressed_counts     :按键对应GPIO口按下的次数,这里用不上
 * @retval      null
 * @par         修改日志 
 *               Ver0.0.1:
                     Helon_Chan, 2018/06/16, 初始化版本\n 
 */
void long_pressed_cb(uint8_t key_num, uint8_t *long_pressed_counts)
{
    uint8_t Task_key_num = 0;
    switch (key_num)
    {
    case BOARD_BUTTON:
        ESP_LOGI("long_pressed_cb", "long press!!!\n");

        Task_key_num = 5;
        break;
    default:
        break;
    }
    xQueueSend(xQueue_user_key_app, (void *)&Task_key_num, (TickType_t)0);
}

void user_key_cd_task(void *arg)
{
    uint8_t key_num;
    for (;;)
    {
        /* 不断从队列中查询是否有消息 */
        if (xQueueReceive(xQueue_user_key_app, &key_num, portMAX_DELAY))
        {
            switch (key_num)
            {
            case 1:
                printf("1 clikc\n");
                //切换继电器
                Key_Switch_Relay();
                break;

            case 2:
                printf("2 clikc\n");

                char utctime[21];
                Read_UTCtime(utctime, sizeof(utctime));
                printf("Read_UTCtime:%s \n", utctime);

                break;

            case 3:
                printf("3 clikc\n");
                vTask_view_Work();

                break;

            case 4:
                printf("4 clikc\n");
                break;

            case 5:
                ble_app_start();
                // printf("5 clikc\n");
                break;

            default:
                break;
            }
        }
    }
}

/** 
 * 用户的按键初始化函数
 * @param[in]   null 
 * @retval      null
 * @par         修改日志 
 *               Ver0.0.1:
                     Helon_Chan, 2018/06/16, 初始化版本\n 
 */
void user_app_key_init(void)
{
    int32_t err_code;
    err_code = user_key_init(gs_m_key_config, BOARD_BUTTON_COUNT, DECOUNE_TIMER, long_pressed_cb, short_pressed_cb);
    ESP_LOGI("user_app_key_init", "user_key_init is %d\n", err_code);
    xQueue_user_key_app = xQueueCreate(1, sizeof(uint8_t));
    xTaskCreate(user_key_cd_task, "user_key_cd_task", 4096, NULL, 8, NULL);
    // xTaskCreate(vTask_view_Work, "vTask_view_Work", 10240, NULL, 5, NULL);
}