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
#include "my_spi_flash.h"
#include "Cache_data.h"

//内存检测
void memory_check_task(void *pvParameter)
{
	while (1)
	{
		ESP_LOGW("memroy check", "%d: - INTERNAL RAM left %dKB", __LINE__,
				 heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
		// ESP_LOGW(MEMORY_CHECK_TAG, "%d: - SPI      RAM left %dkB", __LINE__,
		// 		heap_caps_get_free_size( MALLOC_CAP_SPIRAM )/1024);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
}

void app_main(void)
{
	// nvs_flash_erase();
	xTaskCreate(&memory_check_task, "memory_check_task", 2048, NULL, 5, NULL);
	nvs_flash_init();
	SPI_FLASH_Init();
	// SPIFlash_Test_Process();
	E2prom_Init();
	/******擦除测试******/
	// Erase_Flash_data_test();
	// Raad_flash_Soctor();

	Uart_Init();
	Led_Init();
	Switch_Init();
	user_app_key_init();

	printf("FIRMWARE=%s\n", FIRMWARE);
	Read_Metadate();
	AT24CXX_Read(SERISE_NUM_ADDR, (uint8_t *)SerialNum, SERISE_NUM_LEN);
	printf("SerialNum=%s\n", SerialNum);
	AT24CXX_Read(PRODUCT_ID_ADDR, (uint8_t *)ProductId, PRODUCT_ID_LEN);
	printf("ProductId=%s\n", ProductId);
	AT24CXX_Read(WEB_HOST_ADD, (uint8_t *)WEB_SERVER, WEB_HOST_LEN);
	printf("Host=%s\n", WEB_SERVER);

	/* 判断是否有序列号和product id */
	if ((strlen(SerialNum) == 0) || (strlen(ProductId) == 0) || (strlen(WEB_SERVER) == 0)) //未获取到序列号或productid，未烧写序列号
	{
		while (1)
		{
			ESP_LOGE("Init", "no SerialNum or product id!");
			Led_Status = LED_STA_NOSER; //故障灯
			vTaskDelay(1000 / portTICK_RATE_MS);
		}
	}

	RS485_Init();
	CSE7759B_Init();
	start_ds18b20();
	Start_Cache();
	ble_app_init();
	init_wifi();
	initialise_http(); //须放在 采集任务建立之后
	initialise_mqtt();
}
