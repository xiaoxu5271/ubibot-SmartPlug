
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

//Device Address
#define DEV_ADD 0XA8

#define PRODUCT_ID_LEN 32
#define SERISE_NUM_LEN 16
#define WEB_HOST_LEN 32
#define CHANNEL_ID_LEN 16
#define USER_ID_LEN 48
#define API_KEY_LEN 33

//参数地址
#define PRODUCT_ID_ADDR 0                                //product id
#define SERISE_NUM_ADDR PRODUCT_ID_ADDR + PRODUCT_ID_LEN //serise num
#define WEB_HOST_ADD SERISE_NUM_ADDR + SERISE_NUM_LEN    //web host
#define CHANNEL_ID_ADD WEB_HOST_ADD + WEB_HOST_LEN       //chanel id
#define USER_ID_ADD CHANNEL_ID_ADD + CHANNEL_ID_LEN      //user id
#define API_KEY_ADD USER_ID_ADD + USER_ID_LEN            //user id

#define FN_SET_FLAG_ADD API_KEY_ADD + API_KEY_LEN //metadata setted flag u8
#define FN_DP_ADD FN_SET_FLAG_ADD + 1             //数据发送频率uint32_t
#define FN_485_T_ADD FN_DP_ADD + 4                //4
#define FN_485_WS_ADD FN_485_T_ADD + 4            //4
#define FN_485_LT_ADD FN_485_WS_ADD + 4           //4
#define FN_485_CO2_ADD FN_485_LT_ADD + 4          //4
#define FN_485_TH_ADD FN_485_CO2_ADD + 4          //485温湿度探头uint32_t
#define FN_485_STH_ADD FN_485_TH_ADD + 4          //485 土壤探头uint32_t
#define FN_EXT_ADD FN_485_STH_ADD + 4             //18b20uint32_t
#define FN_ENERGY_ADD FN_EXT_ADD + 4              //电能信息：电压/电流/功率uint32_t
#define FN_ELE_QUAN_ADD FN_ENERGY_ADD + 4         //用电量统计uint32_t
#define CG_DATA_LED_ADD FN_ELE_QUAN_ADD + 4       //发送数据 LED状态 0关闭，1打开uint8_t
#define NET_MODE_ADD CG_DATA_LED_ADD + 1          //上网模式选择 0：自动模式 1：lan模式 2：wifi模式uint8_t
#define DE_SWITCH_STA_ADD NET_MODE_ADD + 1        //uint8_t

#define LAST_SWITCH_ADD DE_SWITCH_STA_ADD + 1 //UINT8

#define WIFI_SSID_ADD LAST_SWITCH_ADD + 1    //32
#define WIFI_PASSWORD_ADD WIFI_SSID_ADD + 32 //64

#define START_READ_NUM_ADD WIFI_PASSWORD_ADD + 64 //数据读取开始地址 uint32_t
#define DATA_SAVE_NUM_ADD START_READ_NUM_ADD + 4  //缓存的数据组数 uint32_t
#define FLASH_USED_NUM_ADD DATA_SAVE_NUM_ADD + 4  //缓存已用大小 u32

#define RSSI_NUM_ADDR FLASH_USED_NUM_ADD + 4          //uint8_t
#define GPRS_RSSI_NUM_ADDR RSSI_NUM_ADDR + 1          //uint8_t
#define RS485_LIGHT_NUM_ADDR GPRS_RSSI_NUM_ADDR + 1   //uint8_t
#define RS485_TEMP_NUM_ADDR RS485_LIGHT_NUM_ADDR + 1  //uint8_t
#define RS485_HUMI_NUM_ADDR RS485_TEMP_NUM_ADDR + 1   //uint8_t
#define RS485_STEMP_NUM_ADDR RS485_HUMI_NUM_ADDR + 1  //uint8_t
#define RS485_SHUMI_NUM_ADDR RS485_STEMP_NUM_ADDR + 1 //uint8_t
#define EXT_TEMP_NUM_ADDR RS485_SHUMI_NUM_ADDR + 1    //uint8_t
#define RS485_T_NUM_ADDR EXT_TEMP_NUM_ADDR + 1        //uint8_t
#define RS485_WS_NUM_ADDR RS485_T_NUM_ADDR + 1        //uint8_t
#define RS485_CO2_NUM_ADDR RS485_WS_NUM_ADDR + 1      //uint8_t
#define RS485_PH_NUM_ADDR RS485_CO2_NUM_ADDR + 1      //uint8_t

void E2prom_Init(void);
esp_err_t AT24CXX_WriteOneByte(uint16_t reg_addr, uint8_t dat);
uint8_t AT24CXX_ReadOneByte(uint16_t reg_addr);
void AT24CXX_WriteLenByte(uint16_t WriteAddr, uint32_t DataToWrite, uint8_t Len);
uint32_t AT24CXX_ReadLenByte(uint16_t ReadAddr, uint8_t Len);
void AT24CXX_Read(uint16_t ReadAddr, uint8_t *pBuffer, uint16_t NumToRead);
void AT24CXX_Write(uint16_t WriteAddr, uint8_t *pBuffer, uint16_t NumToWrite);
void E2prom_empty_all(void);

#endif
