/* SPI Master example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "lwip/def.h"
#include "my_spi_flash.h"

static const char *TAG = "SPI_FLASH_MODE";

/*
 This code displays some fancy graphics on the 320x240 LCD on an ESP-WROVER_KIT board.
 This example demonstrates the use of both spi_device_transmit as well as
 spi_device_queue_trans/spi_device_get_trans_result and pre-transmit callbacks.

 Some info about the ILI9341/ST7789V: It has an C/D line, which is connected to a GPIO here. It expects this
 line to be low for a command and high for data. We use a pre-transmit callback here to control that
 line: every transaction has as the user-definable argument the needed state of the D/C line and just
 before the transaction is sent, the callback will set this line to the correct state.
*/

#define PIN_NUM_CS 5
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK 18

#define W25QXX_CS_L gpio_set_level(PIN_NUM_CS, 0);
#define W25QXX_CS_H gpio_set_level(PIN_NUM_CS, 1);

uint16_t W25QXX_TYPE = 0; //默认是W25Q128

static spi_device_handle_t g_spi;

int VprocHALInit(void)
{
	/*if the customer platform requires any init
    * then implement such init here.
    * Otherwise the implementation of this function is complete
    */
	esp_err_t ret = ESP_OK;

	spi_bus_config_t buscfg = {
		.miso_io_num = PIN_NUM_MISO,
		.mosi_io_num = PIN_NUM_MOSI,
		.sclk_io_num = PIN_NUM_CLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1};

	spi_device_interface_config_t devcfg = {
		.clock_speed_hz = 10 * 1000 * 1000, // Clock out at 10 MHz
		.mode = 0,							// SPI mode 0
		.spics_io_num = -1,					//GPIO_NUM_15,             // CS pin
		.queue_size = 6,					//queue 7 transactions at a time
	};
	//Initialize the SPI bus
	if (g_spi)
	{
		return ret;
	}
	ret = spi_bus_initialize(HSPI_HOST, &buscfg, 0);
	assert(ret == ESP_OK);
	ret = spi_bus_add_device(HSPI_HOST, &devcfg, &g_spi);
	assert(ret == ESP_OK);
	gpio_set_pull_mode(PIN_NUM_CS, GPIO_FLOATING);
	return ret;
}

/*HAL clean up function - To close any open file connection
 * microsemi_spis_tw kernel char driver
 *
 * return: a positive integer value for success, a negative integer value for failure
 */

void VprocHALcleanup(void)
{
	/*if the customer platform requires any cleanup function
    * then implement such function here.
    * Otherwise the implementation of this function is complete
    */
	int ret = 0;
	ret = spi_bus_remove_device(g_spi);
	assert(ret == ESP_OK);
	ret = spi_bus_free(HSPI_HOST);
	assert(ret == ESP_OK);
}

/*Note - These functions are PLATFORM SPECIFIC- They must be modified
 *       accordingly
 **********************************************************************/

/* Vproc_msDelay(): use this function to
 *     force a delay of specified time in resolution of milli-second
 *
 * Input Argument: time in unsigned 16-bit
 * Return: none
 */
/*
void Vproc_msDelay(unsigned short time)
{
    ets_delay_us(time * 1000);
}*/

/* VprocWait(): use this function to
*     force a delay of specified time in resolution of 125 micro-Seconds
*
* Input Argument: time in unsigned 32-bit
* Return: none
*/
/*void VprocWait(unsigned long int time)
{
    ets_delay_us(time * 1000);
}*/

/* This is the platform dependent low level spi
 * function to write 16-bit data to the ZL380xx device
 */
int VprocHALWrite(uint8_t val)
{
	/*Note: Implement this as per your platform*/
	esp_err_t ret;
	spi_transaction_t t;
	// unsigned short data = 0;
	memset(&t, 0, sizeof(t));		//Zero out the transaction
	t.length = sizeof(uint8_t) * 8; //Len is in bytes, transaction length is in bits.

	t.tx_buffer = &val;

	ret = spi_device_transmit(g_spi, &t); //Transmit
	assert(ret == ESP_OK);

	return 0;
}

/* This is the platform dependent low level spi
 * function to read 16-bit data from the ZL380xx device
 */
int VprocHALRead(uint8_t *pVal)
{
	/*Note: Implement this as per your platform*/
	esp_err_t ret;
	spi_transaction_t t;
	// unsigned short data = 0xFFFF;
	uint8_t data1 = 0xFF;

	memset(&t, 0, sizeof(t)); //Zero out the transaction
	/*t.length = sizeof(unsigned short) * 8;
    t.rxlength = sizeof(unsigned short) * 8; //The unit of len is byte, and the unit of length is bit.
    t.rx_buffer = &data;*/
	t.length = sizeof(uint8_t) * 8;
	t.rxlength = sizeof(uint8_t) * 8; //The unit of len is byte, and the unit of length is bit.
	t.rx_buffer = &data1;
	ret = spi_device_transmit(g_spi, &t); //Transmit!

	*pVal = data1;
	//*pVal = ntohs(data);

	assert(ret == ESP_OK);

	return 0;
}

/*
uint8_t SPI_ReadWriteByte(uint8_t cmd_temp)
{
	uint8_t data_temp=0;
    esp_err_t ret;
    spi_transaction_t t;

    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=1*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=&data_temp;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    t.cmd = cmd_temp;
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
    return data_temp;
}*/

//读取W25QXX的状态寄存器
//BIT7  6   5   4   3   2   1   0
//SPR   RV  TB BP2 BP1 BP0 WEL BUSY
//SPR:默认0,状态寄存器保护位,配合WP使用
//TB,BP2,BP1,BP0:FLASH区域写保护设置
//WEL:写使能锁定
//BUSY:忙标记位(1,忙;0,空闲)
//默认:0x00
uint8_t W25QXX_ReadSR(void)
{
	uint8_t byte = 0;
	/*W25QXX_CS_L;                            //使能器件
	SPI_ReadWriteByte(W25X_ReadStatusReg);    //发送读取状态寄存器命令
	byte=SPI_ReadWriteByte(0Xff);             //读取一个字节
	W25QXX_CS_H;                            //取消片选
	*/
	W25QXX_CS_L;
	VprocHALWrite(W25X_ReadStatusReg);
	VprocHALRead(&byte);
	W25QXX_CS_H;

	return byte;
}

//写W25QXX状态寄存器
//只有SPR,TB,BP2,BP1,BP0(bit 7,5,4,3,2)可以写!!!
void W25QXX_Write_SR(uint8_t sr)
{
	W25QXX_CS_L; //使能器件
	//SPI_ReadWriteByte(W25X_WriteStatusReg);   //发送写取状态寄存器命令
	//SPI_ReadWriteByte(sr);               //写入一个字节
	VprocHALWrite(W25X_WriteStatusReg);
	VprocHALWrite(sr);
	W25QXX_CS_H; //取消片选
}
//W25QXX写使能
//将WEL置位
void W25QXX_Write_Enable(void)
{
	W25QXX_CS_L; //使能器件
	//SPI_ReadWriteByte(W25X_WriteEnable);      //发送写使能
	VprocHALWrite(W25X_WriteEnable);
	W25QXX_CS_H; //取消片选
}
//W25QXX写禁止
//将WEL清零
void W25QXX_Write_Disable(void)
{
	W25QXX_CS_L; //使能器件
	//SPI_ReadWriteByte(W25X_WriteDisable);     //发送写禁止指令
	VprocHALWrite(W25X_WriteDisable);
	W25QXX_CS_H; //取消片选
}

//等待空闲
void W25QXX_Wait_Busy(void)
{
	while ((W25QXX_ReadSR() & 0x01) == 0x01)
		; // 等待BUSY位清空
}

//读取芯片ID
//返回值如下:
//0XEF13,表示芯片型号为W25Q80
//0XEF14,表示芯片型号为W25Q16
//0XEF15,表示芯片型号为W25Q32
//0XEF16,表示芯片型号为W25Q64
//0XEF17,表示芯片型号为W25Q128
uint16_t W25QXX_ReadID(void)
{
	uint8_t Temp = 0;
	uint16_t out_Temp = 0;
	/*
	SPI_ReadWriteByte(0x90);//发送读取ID命令
	SPI_ReadWriteByte(0x00);
	SPI_ReadWriteByte(0x00);
	SPI_ReadWriteByte(0x00);
	Temp|=SPI_ReadWriteByte(0xFF)<<8;
	Temp|=SPI_ReadWriteByte(0xFF);*/
	W25QXX_CS_L;
	VprocHALWrite(0x90);
	VprocHALWrite(0);
	VprocHALWrite(0);
	VprocHALWrite(0);
	VprocHALRead(&Temp);
	out_Temp = (uint16_t)Temp << 8;
	VprocHALRead(&Temp);
	out_Temp |= Temp;
	W25QXX_CS_H;
	return out_Temp;
}

//SPI在一页(0~65535)内写入少于256个字节的数据
//在指定地址开始写入最大256字节的数据
//pBuffer:数据存储区
//WriteAddr:开始写入的地址(24bit)
//NumByteToWrite:要写入的字节数(最大256),该数不应该超过该页的剩余字节数!!!
void W25QXX_Write_Page(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
	uint16_t i;
	W25QXX_Write_Enable(); //SET WEL
	W25QXX_CS_L;		   //使能器件
	//SPI_ReadWriteByte(W25X_PageProgram);      //发送写页命令
	VprocHALWrite(W25X_PageProgram);
	//SPI_ReadWriteByte((uint8_t)((WriteAddr)>>16)); //发送24bit地址
	VprocHALWrite((uint8_t)((WriteAddr) >> 16));
	//SPI_ReadWriteByte((uint8_t)((WriteAddr)>>8));
	VprocHALWrite((uint8_t)((WriteAddr) >> 8));
	//SPI_ReadWriteByte((uint8_t)WriteAddr);
	VprocHALWrite((uint8_t)(WriteAddr));
	for (i = 0; i < NumByteToWrite; i++)
	{
		//SPI_ReadWriteByte(pBuffer[i]);//循环写数
		VprocHALWrite(pBuffer[i]);
	}
	W25QXX_CS_H;		//取消片选
	W25QXX_Wait_Busy(); //等待写入结束
}
//无检验写SPI FLASH
//必须确保所写的地址范围内的数据全部为0XFF,否则在非0XFF处写入的数据将失败!
//具有自动换页功能
//在指定地址开始写入指定长度的数据,但是要确保地址不越界!
//pBuffer:数据存储区
//WriteAddr:开始写入的地址(24bit)
//NumByteToWrite:要写入的字节数(最大65535)
//CHECK OK
void W25QXX_Write_NoCheck(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
	uint16_t pageremain;
	pageremain = 256 - WriteAddr % 256; //单页剩余的字节数
	if (NumByteToWrite <= pageremain)
		pageremain = NumByteToWrite; //不大于256个字节
	while (1)
	{
		W25QXX_Write_Page(pBuffer, WriteAddr, pageremain);
		if (NumByteToWrite == pageremain)
			break; //写入结束了
		else	   //NumByteToWrite>pageremain
		{
			pBuffer += pageremain;
			WriteAddr += pageremain;

			NumByteToWrite -= pageremain; //减去已经写入了的字节数
			if (NumByteToWrite > 256)
				pageremain = 256; //一次可以写入256个字节
			else
				pageremain = NumByteToWrite; //不够256个字节了
		}
	};
}

//擦除整个芯片
//等待时间超长...
void W25QXX_Erase_Chip(void)
{
	W25QXX_Write_Enable(); //SET WEL
	W25QXX_Wait_Busy();
	W25QXX_CS_L; //使能器件
	//SPI_ReadWriteByte(W25X_ChipErase);        //发送片擦除命令
	VprocHALWrite(W25X_ChipErase);
	W25QXX_CS_H;		//取消片选
	W25QXX_Wait_Busy(); //等待芯片擦除结束
}
//擦除一个扇区
//Dst_Addr:扇区地址 根据实际容量设置
//擦除一个山区的最少时间:150ms
void W25QXX_Erase_Sector(uint32_t Dst_Addr)
{
	//监视falsh擦除情况,测试用
	// 	printf("fe:%x\r\n",Dst_Addr);
	Dst_Addr *= 4096;
	W25QXX_Write_Enable(); //SET WEL
	W25QXX_Wait_Busy();
	W25QXX_CS_L; //使能器件
	/*SPI_ReadWriteByte(W25X_SectorErase);      //发送扇区擦除指令
	SPI_ReadWriteByte((uint8_t)((Dst_Addr)>>16));  //发送24bit地址
	SPI_ReadWriteByte((uint8_t)((Dst_Addr)>>8));
	SPI_ReadWriteByte((uint8_t)Dst_Addr);*/
	VprocHALWrite(W25X_SectorErase);			//发送扇区擦除指令
	VprocHALWrite((uint8_t)((Dst_Addr) >> 16)); //发送24bit地址
	VprocHALWrite((uint8_t)((Dst_Addr) >> 8));
	VprocHALWrite((uint8_t)Dst_Addr);
	W25QXX_CS_H;		//取消片选
	W25QXX_Wait_Busy(); //等待擦除完成
}

//进入掉电模式
void W25QXX_PowerDown(void)
{
	W25QXX_CS_L; //使能器件
	//SPI_ReadWriteByte(W25X_PowerDown);        //发送掉电命令
	VprocHALWrite(W25X_PowerDown);
	W25QXX_CS_H; //取消片选
}
//唤醒
void W25QXX_WAKEUP(void)
{
	W25QXX_CS_L; //使能器件
	//SPI_ReadWriteByte(W25X_ReleasePowerDown);   //  send W25X_PowerDown command 0xAB
	VprocHALWrite(W25X_ReleasePowerDown);
	W25QXX_CS_H; //取消片选
}

//读取SPI FLASH
//在指定地址开始读取指定长度的数据
//pBuffer:数据存储区
//ReadAddr:开始读取的地址(24bit)
//NumByteToRead:要读取的字节数(最大65535)
void W25QXX_Read(uint8_t *pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead)
{
	uint32_t i;
	uint8_t Temp = 0;
	W25QXX_CS_L;								//使能器件
												/*SPI_ReadWriteByte(W25X_ReadData);         //发送读取命令
    SPI_ReadWriteByte((uint8_t)((ReadAddr)>>16));  //发送24bit地址
    SPI_ReadWriteByte((uint8_t)((ReadAddr)>>8));
    SPI_ReadWriteByte((uint8_t)ReadAddr);
    for(i=0;i<NumByteToRead;i++)
	{
        pBuffer[i]=SPI_ReadWriteByte(0XFF);   //循环读数
    }*/
	VprocHALWrite(W25X_ReadData);				//发送读取命令
	VprocHALWrite((uint8_t)((ReadAddr) >> 16)); //发送24bit地址
	VprocHALWrite((uint8_t)((ReadAddr) >> 8));
	VprocHALWrite((uint8_t)ReadAddr);
	for (i = 0; i < NumByteToRead; i++)
	{
		VprocHALRead(&Temp);
		pBuffer[i] = Temp; //循环读数
	}
	W25QXX_CS_H;
}

//读取数据缓存，返回一组数据占用flash的大小，可能包含错误的部分，失败返回0
//一组数据：{"created_at":"2020-02-20T08:52:08Z","field1":1.001,"field2":1.002,"field2":1.003}
uint16_t W25QXX_Read_Data(uint8_t *Temp_buff, uint32_t ReadAddr, uint16_t Size_Temp_buff)
{
	uint16_t i = 0, j = 0;
	uint8_t Temp = 0;
	bool start_flag = false;
	W25QXX_CS_L; //使能器件

	VprocHALWrite(W25X_ReadData);				//发送读取命令
	VprocHALWrite((uint8_t)((ReadAddr) >> 16)); //发送24bit地址
	VprocHALWrite((uint8_t)((ReadAddr) >> 8));
	VprocHALWrite((uint8_t)ReadAddr);
	for (i = 0; i < Size_Temp_buff; i++)
	{
		VprocHALRead(&Temp);
		if (Temp == '{' && start_flag == false) //数据开头
		{
			Temp_buff[j] = Temp; //循环读数
			start_flag = true;
			j = 1;
		}
		else if (start_flag == true) //开始一组数据计数
		{
			if (Temp == '}') //一组数据结束
			{
				Temp_buff[j] = Temp; //循环读数
				return i;			 //返回从缓存中读了多少字节,用来计算下次读取的开始地址
			}
			else if (Temp == '{') //数据缓存时出现中断，重新计数
			{
				memset(Temp_buff, 0, (j + 1));
				j = 0;
				start_flag = false;
				ESP_LOGE("read_date", "data broke");
			}
			else
			{
				Temp_buff[j] = Temp; //循环读数
				j++;
			}
		}
		else
		{
			ESP_LOGE("read_date", "no data");
		}
	}
	W25QXX_CS_H;
	ESP_LOGE("read_date", "read over ,no correct data");
	return 0;
}

//读取数据缓存中正确的数据的大小，
//一组数据：{"created_at":"2020-02-20T08:52:08Z","field1":1.001,"field2":1.002,"field2":1.003}
uint32_t Read_Post_Len(uint32_t Start_Addr, uint32_t End_Addr)
{
	ESP_LOGI("read_date", "Start_Addr=%d,End_Addr=%d", Start_Addr, End_Addr);
	if (Start_Addr > End_Addr)
	{
		ESP_LOGE("read_date", "Start_Addr>End_Addr");
		return 0;
	}

	uint32_t i = 0, j = 0;
	uint8_t Temp = 0;
	uint32_t post_len = 0;
	uint32_t data_num = 0;
	bool start_flag = false;

	W25QXX_CS_L; //使能器件

	VprocHALWrite(W25X_ReadData);				  //发送读取命令
	VprocHALWrite((uint8_t)((Start_Addr) >> 16)); //发送24bit地址
	VprocHALWrite((uint8_t)((Start_Addr) >> 8));
	VprocHALWrite((uint8_t)Start_Addr);
	for (i = 0; i < (End_Addr - Start_Addr); i++)
	{
		VprocHALRead(&Temp);
		// ESP_LOGI("read_date", "Temp=%c", Temp);

		if (Temp == '{' && start_flag == false) //数据开头
		{
			// Temp_buff[j] = Temp; //循环读数
			start_flag = true;
			j = 1;
			// ESP_LOGI("read_date", "read data start");
		}
		else if (start_flag == true) //开始一组数据计数
		{
			if (Temp == '}') //一组数据结束
			{
				j++;
				post_len = post_len + j + 1; //加上 ',' 分隔
				j = 0;
				start_flag = false;
				data_num++;
				// ESP_LOGI("read_date", "data_num = %d,post_len = %d", data_num, post_len);
			}
			else if (Temp == '{') //数据缓存时出现中断，重新计数
			{
				j = 0;
				start_flag = false;
				ESP_LOGE("read_date", "data broke");
			}
			else
			{
				// Temp_buff[j] = Temp; //循环读数
				j++;
				if (j > 512)
				{
					ESP_LOGE("read_date", "too long ,err");
					j = 0;
					start_flag = false;
				}
			}
		}
		else
		{
			ESP_LOGE("read_date", "no data");
		}
	}
	W25QXX_CS_H;
	return (post_len - 1);
}

//写SPI FLASH
//在指定地址开始写入指定长度的数据
//该函数带擦除操作!
//pBuffer:数据存储区
//WriteAddr:开始写入的地址(24bit)
//NumByteToWrite:要写入的字节数(最大65535)
uint8_t W25QXX_BUFFER[4096];
void W25QXX_Write(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
	uint32_t secpos;
	uint16_t secoff;
	uint16_t secremain;
	uint16_t i;
	uint8_t *W25QXX_BUF;
	W25QXX_BUF = W25QXX_BUFFER;
	secpos = WriteAddr / 4096; //扇区地址
	secoff = WriteAddr % 4096; //在扇区内的偏移
	secremain = 4096 - secoff; //扇区剩余空间大小
	//printf("ad:%X,nb:%X\r\n",WriteAddr,NumByteToWrite);//测试用
	if (NumByteToWrite <= secremain)
		secremain = NumByteToWrite; //不大于4096个字节
	while (1)
	{
		W25QXX_Read(W25QXX_BUF, secpos * 4096, 4096); //读出整个扇区的内容
		for (i = 0; i < secremain; i++)				  //校验数据
		{
			if (W25QXX_BUF[secoff + i] != 0XFF)
				break; //需要擦除
		}
		if (i < secremain) //需要擦除
		{
			W25QXX_Erase_Sector(secpos);	//擦除这个扇区
			for (i = 0; i < secremain; i++) //复制
			{
				W25QXX_BUF[i + secoff] = pBuffer[i];
			}
			W25QXX_Write_NoCheck(W25QXX_BUF, secpos * 4096, 4096); //写入整个扇区
		}
		else
			W25QXX_Write_NoCheck(pBuffer, WriteAddr, secremain); //写已经擦除了的,直接写入扇区剩余区间.
		if (NumByteToWrite == secremain)
			break; //写入结束了
		else	   //写入未结束
		{
			secpos++;   //扇区地址增1
			secoff = 0; //偏移位置为0

			pBuffer += secremain;		 //指针偏移
			WriteAddr += secremain;		 //写地址偏移
			NumByteToWrite -= secremain; //字节数递减
			if (NumByteToWrite > 4096)
				secremain = 4096; //下一个扇区还是写不完
			else
				secremain = NumByteToWrite; //下一个扇区可以写完了
		}
	};
}

void SPI_FLASH_Init(void)
{
	gpio_config_t io_conf;
	//disable interrupt
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	//set as output mode
	io_conf.mode = GPIO_MODE_OUTPUT;
	//bit mask of the pins that you want to set,e.g.GPIO18/19
	io_conf.pin_bit_mask = (1ULL << PIN_NUM_CS);
	//disable pull-down mode
	io_conf.pull_down_en = 0;
	//disable pull-up mode
	io_conf.pull_up_en = 0;
	//configure GPIO with the given settings
	gpio_config(&io_conf);

	VprocHALInit();
	ESP_LOGI(TAG, "SPI_SET 1.0 \n");
	W25QXX_CS_H; //SPI FLASH不选中
	//W25QXX_WAKEUP();
	W25QXX_TYPE = W25QXX_ReadID(); //读取FLASH ID.
	while (W25QXX_TYPE != W25Q128)
	{
		vTaskDelay(1000 / portTICK_RATE_MS);
		W25QXX_TYPE = W25QXX_ReadID(); //读取FLASH ID.
		ESP_LOGE(TAG, "W25QXX_TYPE = %x\n", W25QXX_TYPE);
	}

	W25QXX_Write_Enable(); //SPI_FLASH写使能
	W25QXX_Write_SR(0);
	W25QXX_Wait_Busy();

	ESP_LOGI(TAG, "W25QXX OK！\n");
}

//---------------------------------------------------------------------------------
// 测试程序
//--------------------------------------------------------------------------------
// const uint8_t TEXT_Buffer[] = {"test esp32 spi flash w25q128!!!"};
// const uint8_t TEXT_Buffer2[] = {"123456789"};
// #define SIZE sizeof(TEXT_Buffer)
// #define SIZE2 sizeof(TEXT_Buffer2)
uint8_t datatemp[256] = {0};
uint8_t testtemp[50] = {0};

void SPIFlash_Test_Process(void)
{
	// W25QXX_Write((uint8_t *)TEXT_Buffer2, 4080, SIZE2);
	// W25QXX_Write((uint8_t *)TEXT_Buffer, 4090, SIZE);
	// printf("spi flash test write ok!\n");

	char *OutBuffer;
	uint8_t *SaveBuffer;
	uint8_t len = 0;
	uint16_t read_len;
	cJSON *pJsonRoot;
	pJsonRoot = cJSON_CreateObject();
	cJSON_AddStringToObject(pJsonRoot, "created_at", "2020-02-20T08:52:08Z");
	cJSON_AddNumberToObject(pJsonRoot, "field1", 1.001232);
	cJSON_AddNumberToObject(pJsonRoot, "field2", 1.00234);
	cJSON_AddNumberToObject(pJsonRoot, "field2", 1.04303);
	OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
	cJSON_Delete(pJsonRoot);					   //delete cjson root
	len = strlen(OutBuffer);
	printf("len:%d\n%s\n", len, OutBuffer);
	// SaveBuffer = (uint8_t *)malloc(len);
	SaveBuffer = (uint8_t *)malloc(len);
	memcpy(SaveBuffer, OutBuffer, len);
	W25QXX_Write(SaveBuffer, 0, len);
	free(SaveBuffer);
	free(OutBuffer);

	char *OutBuffer2;
	uint8_t *SaveBuffer2;
	uint8_t len2 = 0;
	cJSON *pJsonRoot2;
	pJsonRoot2 = cJSON_CreateObject();
	cJSON_AddStringToObject(pJsonRoot2, "created_at", "2020-02-20T08:52:08Z");
	cJSON_AddNumberToObject(pJsonRoot2, "field1", 1.004);
	cJSON_AddNumberToObject(pJsonRoot2, "field2", 1.005);
	cJSON_AddNumberToObject(pJsonRoot2, "field2", 1.006);
	OutBuffer2 = cJSON_PrintUnformatted(pJsonRoot2); //cJSON_Print(Root)
	cJSON_Delete(pJsonRoot2);						 //delete cjson root
	len2 = strlen(OutBuffer2);
	printf("len2:%d\n%s\n", len2, OutBuffer2);
	// SaveBuffer = (uint8_t *)malloc(len);
	SaveBuffer2 = (uint8_t *)malloc(len2);
	memcpy(SaveBuffer2, OutBuffer2, len2);
	W25QXX_Write(SaveBuffer2, len, len2);
	free(SaveBuffer2);
	free(OutBuffer2);

	// W25QXX_Read((uint8_t *)datatemp, 0, len2 + len);
	read_len = W25QXX_Read_Data(datatemp, 0, 256);
	printf("read_len1:%d\n%s\n ", read_len, datatemp);
	memset(datatemp, 0, 256);
	read_len = W25QXX_Read_Data(datatemp, read_len + 1, 256);
	printf("read_len2:%d\n%s\n ", read_len, datatemp);
}
