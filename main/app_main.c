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

static void Uart0_Task(void *arg)
{
	while (1)
	{
		Uart0_read();
		vTaskDelay(10 / portTICK_RATE_MS);
	}
}

void app_main(void)
{
	// nvs_flash_erase();
	nvs_flash_init();
	SPI_FLASH_Init();
	// SPIFlash_Test_Process();
	E2prom_Init();
	Uart_Init();
	Led_Init();
	RS485_Init();
	CSE7759B_Init();

	Switch_Init();
	user_app_key_init();

	xTaskCreate(Uart0_Task, "Uart0_Task", 4096, NULL, 9, NULL);

	printf("FIRMWARE=%s\n", FIRMWARE);

	/*step1 判断是否有序列号和product id****/
	AT24CXX_Read(SERISE_NUM_ADDR, (uint8_t *)SerialNum, SERISE_NUM_LEN);
	printf("SerialNum=%s\n", SerialNum);
	AT24CXX_Read(PRODUCT_ID_ADDR, (uint8_t *)ProductId, PRODUCT_ID_LEN);
	printf("ProductId=%s\n", ProductId);
	AT24CXX_Read(WEB_HOST_ADD, (uint8_t *)WEB_SERVER, WEB_HOST_LEN);
	printf("Host=%s\n", WEB_SERVER);
	net_mode = AT24CXX_ReadOneByte(net_mode_add); //读取网络模式
	printf("net mode is %d!\n", net_mode);

	if ((strlen(SerialNum) == 0) || (strlen(ProductId) == 0) || (strlen(WEB_SERVER) == 0)) //未获取到序列号或productid，未烧写序列号
	{
		printf("no SerialNum or product id!\n");
		while (1)
		{
			//故障灯
			Led_Status = LED_STA_NOSER;
			vTaskDelay(500 / portTICK_RATE_MS);
		}
	}

	// strncpy(ble_dev_pwd, SerialNum + 3, 4);
	// printf("ble_dev_pwd=%s\n", ble_dev_pwd);

	ble_app_init();
	init_wifi();

	// xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
	// 					false, true, portMAX_DELAY); //等待网络连接、

	initialise_http();
	initialise_mqtt();
}
