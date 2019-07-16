

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

//page0 用地址
#define ADDR_PAGE0 0xA8
#define PRODUCT_ID_ADDR 0x40
#define SERISE_NUM_ADDR 0x30

//page1 用地址
#define ADDR_PAGE1 0xAA

//page2 用地址
#define ADDR_PAGE2 0xAC
#define need_update_add 0x00     //page2 用地址
#define update_fail_num_add 0x01 //page2 用地址
#define dhcp_mode_add 0x02       //page2 用地址

//page3 用地址
#define ADDR_PAGE3 0xAE
#define ota_url_add 0x00
#define NETINFO_add 0x80

esp_err_t EE_byte_Write(uint8_t page, uint8_t reg_addr, uint8_t dat);
esp_err_t EE_byte_Read(uint8_t page, uint8_t reg_addr, uint8_t *dat);

extern void E2prom_Init(void);
extern int E2prom_Write(uint8_t addr, uint8_t *data_write, int len);
extern int E2prom_Read(uint8_t addr, uint8_t *data_read, int len);
extern int E2prom_BluWrite(uint8_t addr, uint8_t *data_write, int len);
extern int E2prom_BluRead(uint8_t addr, uint8_t *data_read, int len);
extern int E2prom_page_Write(uint8_t addr, uint8_t *data_write, int len);
extern int E2prom_page_Read(uint8_t addr, uint8_t *data_read, int len);

#endif
