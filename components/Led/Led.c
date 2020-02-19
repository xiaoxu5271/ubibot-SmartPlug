#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "Led.h"

#if 1

#define GPIO_LED_B GPIO_NUM_33

#else

#define GPIO_LED_B 21
#define GPIO_LED_G 22
#define GPIO_LED_R 23

#endif

TaskHandle_t Led_Task_Handle = NULL;
uint8_t Last_Led_Status;

static void Led_Task(void *arg)
{
    while (1)
    {
        switch (Led_Status)
        {
        case LED_STA_INIT:
            Led_B_On();
            vTaskDelay(10 / portTICK_RATE_MS);
            break;

        case LED_STA_WORK:
            // Led_G_On();
            // vTaskDelay(100 / portTICK_RATE_MS);
            Led_Off();
            vTaskDelay(100 / portTICK_RATE_MS);
            break;

        case LED_STA_AP:
            Led_B_On();
            vTaskDelay(400 / portTICK_RATE_MS);
            Led_Off();
            vTaskDelay(100 / portTICK_RATE_MS);
            break;

        case LED_STA_NOSER:
            Led_R_On();
            vTaskDelay(100 / portTICK_RATE_MS);
            break;

        case LED_STA_WIFIERR:
            Led_Off();
            vTaskDelay(250 / portTICK_RATE_MS);
            Led_B_On();
            vTaskDelay(250 / portTICK_RATE_MS);
            break;

        case LED_STA_ACTIVE_ERR:
            Led_Off();
            vTaskDelay(250 / portTICK_RATE_MS);
            Led_B_On();
            vTaskDelay(250 / portTICK_RATE_MS);
            break;

        case LED_STA_SEND:
            // Led_Off();
            // vTaskDelay(100 / portTICK_RATE_MS);
            // Led_G_On();
            // vTaskDelay(100 / portTICK_RATE_MS);
            // Led_Off();
            // vTaskDelay(100 / portTICK_RATE_MS);
            // Led_G_On();
            // vTaskDelay(100 / portTICK_RATE_MS);
            // Led_Off();
            // vTaskDelay(100 / portTICK_RATE_MS);

            Led_Status = Last_Led_Status;
            break;

        case LED_STA_HEARD_ERR:
            Led_Off();
            vTaskDelay(250 / portTICK_RATE_MS);
            Led_B_On();
            vTaskDelay(250 / portTICK_RATE_MS);

        case LED_STA_OTA:
            Led_Off();
            vTaskDelay(50 / portTICK_RATE_MS);
            Led_B_On();
            vTaskDelay(50 / portTICK_RATE_MS);

            break;
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
    io_conf.pin_bit_mask = (1ULL << GPIO_LED_B);
    //disable pull-down mode
    io_conf.pull_down_en = 1;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    Led_Status = LED_STA_INIT;

    xTaskCreate(Led_Task, "Led_Task", 4096, NULL, 2, &Led_Task_Handle);
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
    // gpio_set_level(GPIO_LED_R, 0);
    // gpio_set_level(GPIO_LED_G, 0);
    // gpio_set_level(GPIO_LED_B, 1);
}

void Led_C_On(void) //
{
    // gpio_set_level(GPIO_LED_R, 1);
    // gpio_set_level(GPIO_LED_G, 0);
    // gpio_set_level(GPIO_LED_B, 0);
}

void Led_Off(void)
{
    // gpio_set_level(GPIO_LED_R, 1);
    // gpio_set_level(GPIO_LED_G, 1);
    gpio_set_level(GPIO_LED_B, 0);
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
