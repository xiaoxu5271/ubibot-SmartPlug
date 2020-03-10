#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "E2prom.h"
#include "Json_parse.h"
#include "Http.h"
#include "user_key.h"
#include "Led.h"

static const char *TAG = "EEPROM";
SemaphoreHandle_t At24_Mutex = NULL;

static bool AT24CXX_Check(void);
static void E2prom_set_defaul(void);
static void E2prom_read_defaul(void);

// #define TEST_ADDR 4096
// esp_err_t fm24c_test(void)
// {
//     //write
//     int ret = 0;
//     // uint8_t write_buff[256] = {0};
//     // AT24CXX_Write(TEST_ADDR, write_buff, sizeof(write_buff));
//     i2c_cmd_handle_t cmd = i2c_cmd_link_create();
//     i2c_master_start(cmd);

//     i2c_master_write_byte(cmd, DEV_ADD, ACK_CHECK_EN);
//     i2c_master_write_byte(cmd, (TEST_ADDR / 256), ACK_CHECK_EN);
//     i2c_master_write_byte(cmd, (TEST_ADDR % 256), ACK_CHECK_EN);

//     for (uint16_t i = 0; i < 256; i++)
//     {
//         i2c_master_write_byte(cmd, i, ACK_CHECK_EN); //send data value
//     }
//     i2c_master_stop(cmd);
//     ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
//     i2c_cmd_link_delete(cmd);

//     // vTaskDelay(100 / portTICK_PERIOD_MS);
//     // read
//     uint8_t read_temp[256] = {0};
//     uint8_t *p;
//     p = read_temp;
//     cmd = i2c_cmd_link_create();
//     i2c_master_start(cmd);
//     i2c_master_write_byte(cmd, DEV_ADD, ACK_CHECK_EN);
//     i2c_master_write_byte(cmd, (TEST_ADDR & 0xff00) >> 8, ACK_CHECK_EN);
//     i2c_master_write_byte(cmd, TEST_ADDR & 0xff, ACK_CHECK_EN);

//     i2c_master_start(cmd);
//     i2c_master_write_byte(cmd, DEV_ADD + 1, ACK_CHECK_EN);
//     for (uint16_t i = 0; i < 256; i++)
//     {
//         if (i != 255 - 1)
//         {
//             i2c_master_read_byte(cmd, p, ACK_VAL);
//         }
//         else
//         {
//             i2c_master_read_byte(cmd, p, NACK_VAL);
//         }
//         p++;
//     }
//     i2c_master_stop(cmd);
//     ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
//     i2c_cmd_link_delete(cmd);
//     esp_log_buffer_hex("test", read_temp, sizeof(read_temp));

//     // AT24CXX_Read(TEST_ADDR, read_temp, sizeof(read_temp));
//     // esp_log_buffer_hex("test2", read_temp, sizeof(read_temp));
//     return ret;
// }

void E2prom_Init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_MASTER_RX_BUF_DISABLE,
                       I2C_MASTER_TX_BUF_DISABLE, 0);

    At24_Mutex = xSemaphoreCreateMutex();

    while (AT24CXX_Check() != true)
    {
        vTaskDelay(1000 / portTICK_RATE_MS);
        ESP_LOGE(TAG, "eeprom err !");
        Led_Status = LED_STA_HEARD_ERR;
        return;
    }
    // fm24c_test();
    if (Check_Set_defaul())
    {
        Led_Status = LED_STA_REST;
        E2prom_read_defaul();
        E2prom_set_defaul();
        Led_Status = LED_STA_INIT;
    }
}

esp_err_t FM24C_Write(uint16_t reg_addr, uint8_t *dat, uint16_t len)
{
    // xSemaphoreTake(At24_Mutex, -1);
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, DEV_ADD, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (reg_addr & 0xff00) >> 8, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr & 0xff, ACK_CHECK_EN);

    while (len)
    {
        i2c_master_write_byte(cmd, *dat, ACK_CHECK_EN); //send data value
        dat++;
        len--;
    }

    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    // xSemaphoreGive(At24_Mutex);
    return ret;
}

esp_err_t FM24C_Read(uint16_t reg_addr, uint8_t *dat, uint16_t len)
{
    // xSemaphoreTake(At24_Mutex, -1);
    int ret, i;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, DEV_ADD, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (reg_addr & 0xff00) >> 8, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr & 0xff, ACK_CHECK_EN);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DEV_ADD + 1, ACK_CHECK_EN);

    for (i = 0; i < len; i++)
    {
        if (i == len - 1)
        {
            i2c_master_read_byte(cmd, dat, NACK_VAL); //只读1 byte 不需要应答;  //read a byte no ack
        }
        else
        {
            i2c_master_read_byte(cmd, dat, ACK_VAL); //read a byte ack
        }
        dat++;
    }

    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    // xSemaphoreGive(At24_Mutex);
    return ret;
}

esp_err_t AT24CXX_WriteOneByte(uint16_t reg_addr, uint8_t dat)
{
    xSemaphoreTake(At24_Mutex, -1);
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

#ifdef FM24
    i2c_master_write_byte(cmd, DEV_ADD, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (reg_addr & 0xff00) >> 8, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr & 0xff, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, dat, ACK_CHECK_EN);
    // uint8_t data_buff[1];
    // data_buff[0] = dat;
    // FM24C_Write(reg_addr, data_buff, 1);
#else
    i2c_master_write_byte(cmd, (DEV_ADD + ((reg_addr / 256) << 1)), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (reg_addr % 256), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, dat, ACK_CHECK_EN);
#endif

    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
#ifdef FM24
#else
    vTaskDelay(10 / portTICK_RATE_MS);
#endif
    xSemaphoreGive(At24_Mutex);
    return ret;
}

uint8_t AT24CXX_ReadOneByte(uint16_t reg_addr)
{
    xSemaphoreTake(At24_Mutex, -1);
    uint8_t temp = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
#ifdef FM24
    i2c_master_write_byte(cmd, DEV_ADD, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (reg_addr & 0xff00) >> 8, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr & 0xff, ACK_CHECK_EN);
    // FM24C_Read(reg_addr, &temp, 1);
#else
    i2c_master_write_byte(cmd, (DEV_ADD + ((reg_addr / 256) << 1)), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (reg_addr % 256), ACK_CHECK_EN);
#endif

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DEV_ADD + 1, ACK_CHECK_EN);

    i2c_master_read_byte(cmd, &temp, NACK_VAL); //只读1 byte 不需要应答

    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

#ifdef FM24
#else
    vTaskDelay(10 / portTICK_RATE_MS);
#endif

    xSemaphoreGive(At24_Mutex);
    return temp;
}

//在AT24CXX里面的指定地址开始写入长度为Len的数据
//该函数用于写入16bit或者32bit的数据.
//WriteAddr  :开始写入的地址
//DataToWrite:数据数组首地址
//Len        :要写入数据的长度2,4
void AT24CXX_WriteLenByte(uint16_t WriteAddr, uint32_t DataToWrite, uint8_t Len)
{
    uint8_t t;
#ifdef FM24
    uint8_t *data_temp;
    data_temp = (uint8_t *)malloc(Len);

    for (t = 0; t < Len; t++)
    {
        data_temp[t] = (DataToWrite >> (8 * t)) & 0xff;
    }
    FM24C_Write(WriteAddr, data_temp, Len);
    free(data_temp);

#else

    for (t = 0; t < Len; t++)
    {
        AT24CXX_WriteOneByte(WriteAddr + t, (DataToWrite >> (8 * t)) & 0xff);
    }
#endif
}

//在AT24CXX里面的指定地址开始读出长度为Len的数据
//该函数用于读出16bit或者32bit的数据.
//ReadAddr   :开始读出的地址
//返回值     :数据
//Len        :要读出数据的长度2,4
uint32_t AT24CXX_ReadLenByte(uint16_t ReadAddr, uint8_t Len)
{
    uint8_t t;
    uint32_t temp = 0;

#ifdef FM24
    uint8_t *data_temp;
    data_temp = (uint8_t *)malloc(Len);

    FM24C_Read(ReadAddr, data_temp, Len);

    for (t = 0; t < Len; t++)
    {
        temp += (data_temp[t] << (8 * t));
    }
    free(data_temp);
#else
    for (t = 0; t < Len; t++)
    {
        temp <<= 8;
        temp += AT24CXX_ReadOneByte(ReadAddr + Len - t - 1);
    }
#endif

    return temp;
}

//在AT24CXX里面的指定地址开始读出指定个数的数据
//ReadAddr :开始读出的地址 对24c08为0~1023
//pBuffer  :数据数组首地址
//NumToRead:要读出数据的个数
void AT24CXX_Read(uint16_t ReadAddr, uint8_t *pBuffer, uint16_t NumToRead)
{

#ifdef FM24
    FM24C_Read(ReadAddr, pBuffer, NumToRead);
#else
    while (NumToRead)
    {
        *pBuffer++ = AT24CXX_ReadOneByte(ReadAddr++);
        NumToRead--;
    }
#endif
}
//在AT24CXX里面的指定地址开始写入指定个数的数据
//WriteAddr :开始写入的地址 对24c08为0~1023
//pBuffer   :数据数组首地址
//NumToWrite:要写入数据的个数
void AT24CXX_Write(uint16_t WriteAddr, uint8_t *pBuffer, uint16_t NumToWrite)
{
#ifdef FM24
    FM24C_Write(WriteAddr, pBuffer, NumToWrite);
#elif
    while (NumToWrite--)
    {
        AT24CXX_WriteOneByte(WriteAddr, *pBuffer);
        WriteAddr++;
        pBuffer++;
    }
#endif
}

void E2prom_empty_all(void)
{
    printf("\nempty all set\n");
    for (uint16_t i = 0; i < 1024; i++)
    {
        AT24CXX_WriteOneByte(i, 0);
    }
}

static void E2prom_read_defaul(void)
{
    printf("\nread defaul\n");

    AT24CXX_Read(SERISE_NUM_ADDR, (uint8_t *)SerialNum, SERISE_NUM_LEN);
    AT24CXX_Read(PRODUCT_ID_ADDR, (uint8_t *)ProductId, PRODUCT_ID_LEN);
    AT24CXX_Read(WEB_HOST_ADD, (uint8_t *)WEB_SERVER, WEB_HOST_LEN);
    AT24CXX_Read(CHANNEL_ID_ADD, (uint8_t *)ChannelId, CHANNEL_ID_LEN);
}
static void E2prom_set_defaul(void)
{
    E2prom_empty_all();
    //写入默认值
    AT24CXX_Write(SERISE_NUM_ADDR, (uint8_t *)SerialNum, SERISE_NUM_LEN);
    AT24CXX_Write(PRODUCT_ID_ADDR, (uint8_t *)ProductId, PRODUCT_ID_LEN);
    AT24CXX_Write(WEB_HOST_ADD, (uint8_t *)WEB_SERVER, WEB_HOST_LEN);
    AT24CXX_Write(CHANNEL_ID_ADD, (uint8_t *)ChannelId, CHANNEL_ID_LEN);
    AT24CXX_WriteLenByte(FN_DP_ADD, fn_dp, 4);
    AT24CXX_WriteLenByte(FN_ELE_QUAN_ADD, fn_ele_quan, 4);
    AT24CXX_WriteLenByte(FN_ENERGY_ADD, fn_energy, 4);
    AT24CXX_WriteOneByte(CG_DATA_LED_ADD, cg_data_led);
    AT24CXX_WriteOneByte(RSSI_NUM_ADDR, rssi_w_f_num);
    AT24CXX_WriteOneByte(GPRS_RSSI_NUM_ADDR, rssi_g_f_num);
    AT24CXX_WriteOneByte(RS485_LIGHT_NUM_ADDR, r1_light_f_num);
    AT24CXX_WriteOneByte(RS485_TEMP_NUM_ADDR, r1_th_t_f_num);
    AT24CXX_WriteOneByte(RS485_HUMI_NUM_ADDR, r1_th_h_f_num);
    AT24CXX_WriteOneByte(RS485_STEMP_NUM_ADDR, r1_sth_t_f_num);
    AT24CXX_WriteOneByte(RS485_SHUMI_NUM_ADDR, r1_sth_h_f_num);
    AT24CXX_WriteOneByte(EXT_TEMP_NUM_ADDR, e1_t_f_num);
    AT24CXX_WriteOneByte(RS485_T_NUM_ADDR, r1_t_f_num);
    AT24CXX_WriteOneByte(RS485_WS_NUM_ADDR, r1_ws_f_num);
    AT24CXX_WriteOneByte(RS485_CO2_NUM_ADDR, r1_co2_f_num);
    AT24CXX_WriteOneByte(RS485_PH_NUM_ADDR, r1_ph_f_num);
}

//检查AT24CXX是否正常,以及是否为新EEPROM
//这里用了24XX的最后一个地址(1023)来存储标志字.
//如果用其他24C系列,这个地址要修改

static bool AT24CXX_Check(void)
{
    uint8_t temp;
    temp = AT24CXX_ReadOneByte(E2P_SIZE - 1);
    if (temp == 0xff)
    {
        temp = AT24CXX_ReadOneByte(E2P_SIZE - 2);
        if (temp == 0xff)
        {
            printf("\nnew eeprom\n");
            E2prom_set_defaul();
        }
    }

    if (temp == 0X55)
    {
        printf("eeprom check ok!\n");
        return true;
    }
    else //排除第一次初始化的情况
    {
        AT24CXX_WriteOneByte(E2P_SIZE - 1, 0X55);
        temp = AT24CXX_ReadOneByte(E2P_SIZE - 1);
        if (temp == 0X55)
        {
            printf("eeprom check ok!\n");
            return true;
        }
    }
    printf("eeprom check fail!\n");
    return false;
}