
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
//地址
#define PRODUCT_ID_ADDR 0                                //product id
#define SERISE_NUM_ADDR PRODUCT_ID_ADDR + PRODUCT_ID_LEN //serise num
#define WEB_HOST_ADD SERISE_NUM_ADDR + SERISE_NUM_LEN    //web host
#define CHANNEL_ID_ADD WEB_HOST_ADD + WEB_HOST_LEN       //chanel id
#define USER_ID_ADD CHANNEL_ID_ADD + CHANNEL_ID_LEN      //user id
#define dhcp_mode_add USER_ID_ADD + USER_ID_LEN          //dhcp mode u1
#define net_mode_add dhcp_mode_add + 1                   //net mode u1
#define save_data_add net_mode_add + 4                   //flash save add u32

void E2prom_Init(void);
esp_err_t AT24CXX_WriteOneByte(uint16_t reg_addr, uint8_t dat);
uint8_t AT24CXX_ReadOneByte(uint16_t reg_addr);
void AT24CXX_WriteLenByte(uint16_t WriteAddr, uint32_t DataToWrite, uint8_t Len);
uint32_t AT24CXX_ReadLenByte(uint16_t ReadAddr, uint8_t Len);
void AT24CXX_Read(uint16_t ReadAddr, uint8_t *pBuffer, uint16_t NumToRead);
void AT24CXX_Write(uint16_t WriteAddr, uint8_t *pBuffer, uint16_t NumToWrite);
bool AT24CXX_Check(void);
void E2prom_empty_all(void);

#endif
