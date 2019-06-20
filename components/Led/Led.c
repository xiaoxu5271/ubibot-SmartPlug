#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "Led.h"

//#define GPIO_LED_B    (GPIO_NUM_32)
#define GPIO_LED_Y    (GPIO_NUM_33)

static void Led_Task(void* arg)
{
    while(1)
    {
        switch(Led_Status)
        {
            case LED_STA_INIT:
    
                Led_Y_On();
                vTaskDelay(10 / portTICK_RATE_MS);
                break;
            
            case LED_STA_TOUCH:
                Led_Y_On();
                vTaskDelay(300 / portTICK_RATE_MS);
      
  
                break;
            
            case LED_STA_NOSER:
                Led_Y_On();
                vTaskDelay(10 / portTICK_RATE_MS);
                break;
            
            case LED_STA_WIFIERR:
                Led_Y_On();
                vTaskDelay(300 / portTICK_RATE_MS);
                Led_Off();
                vTaskDelay(300 / portTICK_RATE_MS);
                break;

            case LED_STA_SENDDATA:
                Led_Y_On();
                vTaskDelay(200 / portTICK_RATE_MS);
                Led_Off();
                Led_Status=LED_STA_DATAOVER;
                break;

            case LED_STA_DATAOVER:
                Led_Off();
                vTaskDelay(10 / portTICK_RATE_MS);
                break;

            case LED_STA_RECVDATA:
                Led_Y_On();
                vTaskDelay(100 / portTICK_RATE_MS);
                Led_Off();
                vTaskDelay(100 / portTICK_RATE_MS);
                
                Led_Y_On();
                vTaskDelay(100 / portTICK_RATE_MS);
                Led_Off();               
                Led_Status=LED_STA_DATAOVER;
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
    io_conf.pin_bit_mask = (1ULL<<GPIO_LED_Y);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 1;
    //configure GPIO with the given settings
    gpio_config(&io_conf);  

    Led_Status=LED_STA_INIT;

    xTaskCreate(Led_Task, "Led_Task", 4096, NULL, 5, NULL);

}


void Led_Y_On(void)
{
    gpio_set_level(GPIO_LED_Y, 1);
    
}




void Led_Off(void)
{
    gpio_set_level(GPIO_LED_Y, 0);
   
}


