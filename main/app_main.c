#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"

// #include "nvs.h"
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
#include "EC20.h"

void app_main(void)
{
	Cache_muxtex = xSemaphoreCreateMutex();
	xMutex_Http_Send = xSemaphoreCreateMutex(); //创建HTTP发送互斥信号
	EC20_muxtex = xSemaphoreCreateMutex();
	Net_sta_group = xEventGroupCreate();
	Send_Mqtt_Queue = xQueueCreate(1, sizeof(Mqtt_Msg));

	Led_Init();
	Switch_Init();
	E2prom_Init();
	Read_Metadate_E2p();
	Read_Product_E2p();
	Read_Fields_E2p();
	SPI_FLASH_Init();
	Uart_Init();
	user_app_key_init();

	esp_err_t ret;
	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ble_app_init();
	init_wifi();
	EC20_Init();

	RS485_Init();
	CSE7759B_Init();
	start_ds18b20();
	Start_Cache();
	initialise_http(); //须放在 采集任务建立之后
	initialise_mqtt();

	/* 判断是否有序列号和product id */
	if ((strlen(SerialNum) == 0) || (strlen(ProductId) == 0) || (strlen(WEB_SERVER) == 0)) //未获取到序列号或productid，未烧写序列号
	{
		while (1)
		{
			ESP_LOGE("Init", "no SerialNum or product id!");
			No_ser_flag = true;
			vTaskDelay(1000 / portTICK_RATE_MS);
		}
	}
}
