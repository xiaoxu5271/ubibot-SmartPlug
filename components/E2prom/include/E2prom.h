
#ifndef _E2PROM_H_
#define _E2PROM_H_

#include "freertos/FreeRTOS.h"

#if 1
#define I2C_MASTER_SCL_IO 14     /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 27     /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_1 /*!< I2C port number for master dev */

#else
#define I2C_MASTER_SCL_IO 19     /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 18     /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_1 /*!< I2C port number for master dev */

#endif

#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ 100000   /*!< I2C master clock frequency */

#define ACK_CHECK_EN 0x1  /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0 /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0       /*!< I2C ack value */
#define NACK_VAL 0x1      /*!< I2C nack value */

#define FE_E2P_SIZE 4 * 1024
#define FE_DEV_ADD 0XAE

#define AT_E2P_SIZE 1024
#define AT_DEV_ADD 0XA8

#define PRODUCT_ID_LEN 32
#define SERISE_NUM_LEN 16
#define WEB_HOST_LEN 32
#define CHANNEL_ID_LEN 16
#define USER_ID_LEN 48
#define API_KEY_LEN 33

//参数地址
#define PRODUCT_ID_ADDR 0                                    //product id
#define SERISE_NUM_ADDR PRODUCT_ID_ADDR + PRODUCT_ID_LEN + 1 //serise num
#define WEB_HOST_ADD SERISE_NUM_ADDR + SERISE_NUM_LEN + 1    //web host
#define CHANNEL_ID_ADD WEB_HOST_ADD + WEB_HOST_LEN + 1       //chanel id
#define USER_ID_ADD CHANNEL_ID_ADD + CHANNEL_ID_LEN + 1      //user id
#define API_KEY_ADD USER_ID_ADD + USER_ID_LEN + 1            //user id

#define FN_SET_FLAG_ADD API_KEY_ADD + API_KEY_LEN + 1 //metadata setted flag u8
#define FN_DP_ADD FN_SET_FLAG_ADD + 1 + 1             //数据发送频率uint32_t
#define FN_485_T_ADD FN_DP_ADD + 4 + 1                //4
#define FN_485_WS_ADD FN_485_T_ADD + 4 + 1            //4
#define FN_485_LT_ADD FN_485_WS_ADD + 4 + 1           //4
#define FN_485_CO2_ADD FN_485_LT_ADD + 4 + 1          //4
#define FN_485_TH_ADD FN_485_CO2_ADD + 4 + 1          //485温湿度探头uint32_t
#define FN_485_STH_ADD FN_485_TH_ADD + 4 + 1          //485 土壤探头uint32_t
#define FN_EXT_ADD FN_485_STH_ADD + 4 + 1             //18b20uint32_t
#define FN_ENERGY_ADD FN_EXT_ADD + 4 + 1              //电能信息：电压/电流/功率uint32_t
#define FN_ELE_QUAN_ADD FN_ENERGY_ADD + 4 + 1         //用电量统计uint32_t
#define CG_DATA_LED_ADD FN_ELE_QUAN_ADD + 4 + 1       //发送数据 LED状态 0关闭，1打开uint8_t
#define NET_MODE_ADD CG_DATA_LED_ADD + 1 + 1          //上网模式选择 0：自动模式 1：lan模式 2：wifi模式uint8_t
#define DE_SWITCH_STA_ADD NET_MODE_ADD + 1 + 1        //uint8_t

#define LAST_SWITCH_ADD DE_SWITCH_STA_ADD + 1 + 1 //UINT8

#define WIFI_SSID_ADD LAST_SWITCH_ADD + 1 + 1    //32
#define WIFI_PASSWORD_ADD WIFI_SSID_ADD + 32 + 1 //64

#define START_READ_NUM_ADD WIFI_PASSWORD_ADD + 64 + 1 //数据读取开始地址 uint32_t
#define DATA_SAVE_NUM_ADD START_READ_NUM_ADD + 4 + 1  //缓存的数据组数 uint32_t
#define FLASH_USED_NUM_ADD DATA_SAVE_NUM_ADD + 4 + 1  //缓存已用大小 u32
#define EXHAUSTED_FLAG_ADD FLASH_USED_NUM_ADD + 4 + 1 //整个储存空间达到上限标志 1byte

#define RSSI_NUM_ADDR EXHAUSTED_FLAG_ADD + 1 + 1          //uint8_t
#define GPRS_RSSI_NUM_ADDR RSSI_NUM_ADDR + 1 + 1          //uint8_t
#define RS485_LIGHT_NUM_ADDR GPRS_RSSI_NUM_ADDR + 1 + 1   //uint8_t
#define RS485_TEMP_NUM_ADDR RS485_LIGHT_NUM_ADDR + 1 + 1  //uint8_t
#define RS485_HUMI_NUM_ADDR RS485_TEMP_NUM_ADDR + 1 + 1   //uint8_t
#define RS485_STEMP_NUM_ADDR RS485_HUMI_NUM_ADDR + 1 + 1  //uint8_t
#define RS485_SHUMI_NUM_ADDR RS485_STEMP_NUM_ADDR + 1 + 1 //uint8_t
#define EXT_TEMP_NUM_ADDR RS485_SHUMI_NUM_ADDR + 1 + 1    //uint8_t
#define RS485_T_NUM_ADDR EXT_TEMP_NUM_ADDR + 1 + 1        //uint8_t
#define RS485_WS_NUM_ADDR RS485_T_NUM_ADDR + 1 + 1        //uint8_t
#define RS485_CO2_NUM_ADDR RS485_WS_NUM_ADDR + 1 + 1      //uint8_t
#define RS485_PH_NUM_ADDR RS485_CO2_NUM_ADDR + 1 + 1      //uint8_t
#define SW_S_F_NUM_ADDR RS485_PH_NUM_ADDR + 1 + 1         //uint8_t
#define SW_V_F_NUM_ADDR SW_S_F_NUM_ADDR + 1 + 1           //uint8_t
#define SW_C_F_NUM_ADDR SW_V_F_NUM_ADDR + 1 + 1           //uint8_t
#define SW_P_F_NUM_ADDR SW_C_F_NUM_ADDR + 1 + 1           //uint8_t
#define SW_PC_F_NUM_ADDR SW_P_F_NUM_ADDR + 1 + 1          //uint8_t
#define APN_ADDR SW_PC_F_NUM_ADDR + 1 + 1                 //32 B
#define BEARER_USER_ADDR APN_ADDR + 32 + 1                //32 B
#define BEARER_PWD_ADDR BEARER_USER_ADDR + 32 + 1         //32 B
#define RS485_CO2_T_NUM_ADDR BEARER_PWD_ADDR + 32 + 1     //uint8_t
#define RS485_CO2_H_NUM_ADDR RS485_CO2_T_NUM_ADDR + 1 + 1 //uint8_t

#define WEB_PORT_ADD RS485_CO2_H_NUM_ADDR + 1 + 1      //web PORT
#define MQTT_HOST_ADD WEB_PORT_ADD + 5 + 1             //MQTT HOST
#define MQTT_PORT_ADD MQTT_HOST_ADD + WEB_HOST_LEN + 1 //MQTT PORT

#define FN_SW_ON_ADD MQTT_PORT_ADD + 5 + 1    //4 B
#define SW_ON_F_NUM_ADDR FN_SW_ON_ADD + 4 + 1 //1 B

#define F1_A_ADDR SW_ON_F_NUM_ADDR + 4 + 1
#define F1_B_ADDR F1_A_ADDR + 4 + 1
#define F2_A_ADDR F1_B_ADDR + 4 + 1
#define F2_B_ADDR F2_A_ADDR + 4 + 1
#define F3_A_ADDR F2_B_ADDR + 4 + 1
#define F3_B_ADDR F3_A_ADDR + 4 + 1
#define F4_A_ADDR F3_B_ADDR + 4 + 1
#define F4_B_ADDR F4_A_ADDR + 4 + 1
#define F5_A_ADDR F4_B_ADDR + 4 + 1
#define F5_B_ADDR F5_A_ADDR + 4 + 1
#define F6_A_ADDR F5_B_ADDR + 4 + 1
#define F6_B_ADDR F6_A_ADDR + 4 + 1
#define F7_A_ADDR F6_B_ADDR + 4 + 1
#define F7_B_ADDR F7_A_ADDR + 4 + 1
#define F8_A_ADDR F7_B_ADDR + 4 + 1
#define F8_B_ADDR F8_A_ADDR + 4 + 1
#define F9_A_ADDR F8_B_ADDR + 4 + 1
#define F9_B_ADDR F9_A_ADDR + 4 + 1
#define F10_A_ADDR F9_B_ADDR + 4 + 1
#define F10_B_ADDR F10_A_ADDR + 4 + 1
#define F11_A_ADDR F10_B_ADDR + 4 + 1
#define F11_B_ADDR F11_A_ADDR + 4 + 1
#define F12_A_ADDR F11_B_ADDR + 4 + 1
#define F12_B_ADDR F12_A_ADDR + 4 + 1
#define F13_A_ADDR F12_B_ADDR + 4 + 1
#define F13_B_ADDR F13_A_ADDR + 4 + 1
#define F14_A_ADDR F13_B_ADDR + 4 + 1
#define F14_B_ADDR F14_A_ADDR + 4 + 1
#define F15_A_ADDR F14_B_ADDR + 4 + 1
#define F15_B_ADDR F15_A_ADDR + 4 + 1
#define F16_A_ADDR F15_B_ADDR + 4 + 1
#define F16_B_ADDR F16_A_ADDR + 4 + 1
#define F17_A_ADDR F16_B_ADDR + 4 + 1
#define F17_B_ADDR F17_A_ADDR + 4 + 1
#define F18_A_ADDR F17_B_ADDR + 4 + 1
#define F18_B_ADDR F18_A_ADDR + 4 + 1
#define F19_A_ADDR F18_B_ADDR + 4 + 1
#define F19_B_ADDR F19_A_ADDR + 4 + 1
#define F20_A_ADDR F19_B_ADDR + 4 + 1
#define F20_B_ADDR F20_A_ADDR + 4 + 1

#define FN_485_IS_ADDR F20_B_ADDR + 4 + 1
#define R1_IS_C2H4_F_NUM_ADDR FN_485_IS_ADDR + 4 + 1
#define R1_IS_O2_F_NUM_ADDR R1_IS_C2H4_F_NUM_ADDR + 1 + 1

#define E2P_USAGED R1_IS_O2_F_NUM_ADDR

void E2prom_Init(void);
esp_err_t E2P_WriteOneByte(uint16_t reg_addr, uint8_t dat);
uint8_t E2P_ReadOneByte(uint16_t reg_addr);
void E2P_WriteLenByte(uint16_t WriteAddr, uint32_t DataToWrite, uint8_t Len);
uint32_t E2P_ReadLenByte(uint16_t ReadAddr, uint8_t Len);
void E2P_Read(uint16_t ReadAddr, uint8_t *pBuffer, uint16_t NumToRead);
void E2P_Write(uint16_t WriteAddr, uint8_t *pBuffer, uint16_t NumToWrite);
void E2prom_empty_all(bool flag);
void E2prom_set_defaul(bool flag);

esp_err_t Nvs_Write_32(const char *Key, uint32_t dat);
uint32_t Nvs_Read_32(const char *Key);

#endif
