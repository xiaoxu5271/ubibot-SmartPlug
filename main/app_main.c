#include <stdio.h>
#include <string.h>

#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "Smartconfig.h"
#include "Http.h"
#include "Mqtt.h"
#include "Bluetooth.h"
#include "Json_parse.h"
#include "esp_log.h"

#include "Uart0.h"
#include "RS485_Read.h"
#include "CSE7759B.h"
#include "Led.h"
#include "E2prom.h"
#include "RtcUsr.h"
#include "Switch.h"
#include "ota.h"
#include "ds18b20.h"
#include "user_app.h"

static void Uart0_Task(void *arg)
{
	while (1)
	{
		Uart0_read();
		vTaskDelay(10 / portTICK_RATE_MS);
	}
}

/*
  EEPROM PAGE0 
    0x00 APIkey(32byte)
    0x20 chnnel_id(4byte)
    0x30 Serial_No(16byte)
    0x40 Protuct_id(32byte)
  EEPROM PAGE1+PAGE2
    0X00 bluesave  (512byte)

  */

void app_main(void)
{
	// nvs_flash_erase();
	nvs_flash_init();

	Led_Init();
	RS485_Init();
	CSE7759B_Init();
	E2prom_Init();
	Uart0_Init();
	Switch_Init();
	user_app_key_init();

	xTaskCreate(Uart0_Task, "Uart0_Task", 4096, NULL, 9, NULL);

	/*step1 判断是否有序列号和product id****/
	E2prom_Read(0x30, (uint8_t *)SerialNum, 16);
	printf("SerialNum=%s\n", SerialNum);
	printf("SerialNum_size=%d\n", sizeof(SerialNum));

	E2prom_Read(0x40, (uint8_t *)ProductId, 32);
	printf("ProductId=%s\n", ProductId);

	if ((SerialNum[0] == 0xff) && (SerialNum[1] == 0xff)) //新的eeprom，先清零
	{
		printf("new eeprom\n");
		char zero_data[256];
		bzero(zero_data, sizeof(zero_data));
		E2prom_Write(0x00, (uint8_t *)zero_data, 256);
		E2prom_BluWrite(0x00, (uint8_t *)zero_data, 256); //清空蓝牙

		E2prom_Read(0x30, (uint8_t *)SerialNum, 16);
		printf("SerialNum=%s\n", SerialNum);

		E2prom_Read(0x40, (uint8_t *)ProductId, 32);
		printf("ProductId=%s\n", ProductId);
	}

	if ((strlen(SerialNum) == 0) || (strlen(ProductId) == 0)) //未获取到序列号或productid，未烧写序列号
	{
		printf("no SerialNum or product id!\n");
		while (1)
		{
			//故障灯闪烁
			Led_Status = LED_STA_NOSER;
			vTaskDelay(500 / portTICK_RATE_MS);
		}
	}

	ble_app_start();
	init_wifi();

	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
						false, true, portMAX_DELAY); //等待网络连接、

	/*******************************timer 1s init**********************************************/
	// esp_err_t err = esp_timer_create(&timer_periodic_arg, &timer_periodic_handle);
	// err = esp_timer_start_periodic(timer_periodic_handle, 100 * 1000); //创建定时器，单位us，定时1ms
	// if (err != ESP_OK)
	// {
	//     printf("timer periodic create err code:%d\n", err);
	// }

	initialise_http();
	initialise_mqtt();
}
