#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "Smartconfig.h"
#include "Json_parse.h"
#include "esp_log.h"

#include "Led.h"

#define TAG "LED"

#if 1

#define GPIO_LED_B GPIO_NUM_33
#define GPIO_LED_Y GPIO_NUM_13

#else

#define GPIO_LED_B 21
#define GPIO_LED_G 22
#define GPIO_LED_R 23

#endif

TaskHandle_t Led_Task_Handle = NULL;
uint8_t Last_Led_Status;

bool CSE_FLAG = false;
bool E2P_FLAG = false;
bool FLASH_FLAG = false;

bool Set_defaul_flag = false;
bool Net_sta_flag = false;
bool Cnof_net_flag = false;
bool No_ser_flag = false;

/*******************************************
 硬件错误：黄灯闪烁
 恢复出厂：蓝灯闪烁
 无序列号：黄+蓝 闪烁
 配置网络：黄蓝交替闪烁

 开：蓝灯亮，关：蓝灯灭
 网络正常：黄灯灭，网络问题：黄灯亮
 
 * ***************************************/
static void Led_Task(void *arg)
{
    while (1)
    {
        //硬件错误
        if ((CSE_FLAG == false) || (E2P_FLAG == false) || (FLASH_FLAG == false))
        {
            ESP_LOGI(TAG, "CSE_FLAG=:%d,E2P_FLAG=:%d,FLASH_FLAG=:%d", CSE_FLAG, E2P_FLAG, FLASH_FLAG);
            Led_Off();
            vTaskDelay(500 / portTICK_RATE_MS);
            Led_Y_On();
            vTaskDelay(500 / portTICK_RATE_MS);
        }
        //恢复出厂
        else if (Set_defaul_flag == true)
        {
            Led_Off();
            vTaskDelay(500 / portTICK_RATE_MS);
            Led_B_On();
            vTaskDelay(500 / portTICK_RATE_MS);
        }
        //无序列号
        else if (No_ser_flag == true)
        {
            Led_Off();
            vTaskDelay(500 / portTICK_RATE_MS);
            Led_B_On();
            Led_Y_On();
            vTaskDelay(500 / portTICK_RATE_MS);
        }
        //配网
        else if (Cnof_net_flag == 1)
        {
            Led_Y_Off();
            Led_B_On();
            vTaskDelay(500 / portTICK_RATE_MS);
            Led_Y_On();
            Led_B_Off();
            vTaskDelay(500 / portTICK_RATE_MS);
        }
        //开关以及网络
        else
        {
            //开关
            if (mqtt_json_s.mqtt_switch_status == true)
            {
                Led_B_On();
            }
            else
            {
                Led_B_Off();
            }
            //网络
            if (Net_sta_flag == true)
            {
                Led_Y_Off();
            }
            else
            {
                Led_Y_On();
            }
            vTaskDelay(100 / portTICK_RATE_MS);
        }
    }
}

void Led_Init(void)
{
    gpio_config_t io_conf;

    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO16
    io_conf.pin_bit_mask = (1ULL << GPIO_LED_B) | (1ULL << GPIO_LED_Y);
    //disable pull-down mode
    io_conf.pull_down_en = 1;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    xTaskCreate(Led_Task, "Led_Task", 3072, NULL, 2, &Led_Task_Handle);
}

void Led_R_On(void)
{
    // gpio_set_level(GPIO_LED_R, 0);
    // gpio_set_level(GPIO_LED_G, 1);
    // gpio_set_level(GPIO_LED_B, 1);
}

void Led_G_On(void)
{
    // gpio_set_level(GPIO_LED_R, 1);
    // gpio_set_level(GPIO_LED_G, 0);
    // gpio_set_level(GPIO_LED_B, 1);
}

void Led_B_On(void)
{
    // gpio_set_level(GPIO_LED_R, 1);
    // gpio_set_level(GPIO_LED_G, 1);
    gpio_set_level(GPIO_LED_B, 1);
}

void Led_B_Off(void)
{
    gpio_set_level(GPIO_LED_B, 0);
}

void Led_Y_On(void)
{
    gpio_set_level(GPIO_LED_Y, 1);
}
void Led_Y_Off(void)
{
    gpio_set_level(GPIO_LED_Y, 0);
}

void Led_C_On(void) //
{
    // gpio_set_level(GPIO_LED_R, 1);
    // gpio_set_level(GPIO_LED_G, 0);
    // gpio_set_level(GPIO_LED_B, 0);
}

void Led_Off(void)
{
    Led_B_Off();
    Led_Y_Off();
}

void Turn_Off_LED(void)
{
    vTaskSuspend(Led_Task_Handle);
    Led_Off();
}

void Turn_ON_LED(void)
{
    vTaskResume(Led_Task_Handle);
}
