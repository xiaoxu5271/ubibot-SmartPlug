

#ifndef _EC20_H_
#define _EC20_H_

#include "freertos/FreeRTOS.h"

SemaphoreHandle_t EC20_muxtex;

enum recv_mode
{
    EC_NORMAL = 0,
    EC_OTA,
    EC_INPORT
};

enum ec_err_code
{
    NO_ARK = 401,
    CPIN_ERR,
    QICSGP_ERR,
    CGATT_ERR,
    QIACT_ERR,
    NO_NET
};

extern TaskHandle_t EC20_Task_Handle;
extern TaskHandle_t Uart1_Task_Handle;

extern char ICCID[24];

void EC20_Start(void);
char *AT_Cmd_Send(char *cmd_buf, char *check_buff, uint32_t time_out, uint8_t try_num);
uint8_t EC20_Net_Check(void);
void EC20_Init(void);
void EC20_Start(void);
void EC20_Stop(void);
uint8_t EC20_Http_CFG(void);
uint8_t EC20_Active(char *active_url, char *recv_buf);
uint8_t EC20_Send_Post_Data(char *post_buf, bool end_flag);
uint8_t EC20_Read_Post_Data(char *recv_buff, uint16_t buff_size);
uint8_t EC20_MQTT_INIT(void);
uint8_t EC20_MQTT_PUB(char *data_buff);
uint8_t EC20_Get_Rssi(float *Rssi_val);
void EC20_Power_Off(void);
uint16_t Read_OTA_File(uint8_t flie_handle, char *file_buff);
uint16_t Read_TCP_OTA_File(char *file_buff);
uint8_t Start_EC_OTA(char *ota_url, uint32_t *file_len);
bool Start_EC20_TCP_OTA(void);
bool End_EC_OTA(uint8_t file_handle);
bool End_EC_TCP_OTA(void);
void Check_Module(void);

#endif
