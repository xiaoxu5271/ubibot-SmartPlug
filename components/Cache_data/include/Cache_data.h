
#ifndef _CACHE_H_
#define _CACHE_H_

#include "freertos/FreeRTOS.h"

//单次最大发送的数据条数
#define MAX_READ_NUM 2000

extern SemaphoreHandle_t Cache_muxtex;

extern uint32_t flash_used_num; //数据缓存的截至地址
extern uint32_t last_save_num;  //最后一条数据的起始地址
extern uint32_t start_read_num; //读取数据的开始地址
extern bool Exhausted_flag;     //整个flash 已全部使用标志
// extern uint32_t data_save_num;  //缓存的数据组数

void DataSave(uint8_t *sava_buff, uint16_t Buff_len);
void Start_Cache(void);
void Erase_Flash_data_test(void);
void Raad_flash_Soctor(void);

void Write_Flash_err_Test(void);

#endif
