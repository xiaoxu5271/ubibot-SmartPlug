#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "Json_parse.h"
#include "Http.h"
#include "user_key.h"
#include "Led.h"
#include "crc8_16.h"
#include "ota.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "E2prom.h"

nvs_handle_t nvs_cache_handle;

static const char *TAG = "EEPROM";
SemaphoreHandle_t At24_Mutex = NULL;

uint8_t E2P_M = 0; //0 AT24 ,1 FE
uint16_t E2P_SIZE = 0;
uint8_t DEV_ADD = 0;

static bool
E2P_Check(void);
// static void E2prom_read_defaul(void);

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

    while (E2P_Check() != true)
    {
        vTaskDelay(1000 / portTICK_RATE_MS);
        ESP_LOGE(TAG, "eeprom err !");
        E2P_FLAG = false;
        return;
    }
    E2P_FLAG = true;
}

esp_err_t AT24CXX_WriteOneByte(uint16_t reg_addr, uint8_t dat)
{
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DEV_ADD + ((reg_addr / 256) << 1)), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (reg_addr % 256), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, dat, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    vTaskDelay(20 / portTICK_RATE_MS);
    return ret;
}

esp_err_t AT24CXX_ReadOneByte(uint16_t reg_addr, uint8_t *data)
{
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DEV_ADD + ((reg_addr / 256) << 1)), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (reg_addr % 256), ACK_CHECK_EN);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DEV_ADD + ((reg_addr / 256) << 1)) + 1, ACK_CHECK_EN);

    i2c_master_read_byte(cmd, data, NACK_VAL); //只读1 byte 不需要应答

    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    vTaskDelay(20 / portTICK_RATE_MS);

    return ret;
}

esp_err_t AT24_Write(uint16_t reg_addr, uint8_t *dat, uint16_t len)
{
    esp_err_t ret;
    xSemaphoreTake(At24_Mutex, -1);
    uint8_t t;
    for (t = 0; t < len; t++)
    {
        ret = AT24CXX_WriteOneByte(reg_addr + t, *dat);
        dat++;
        if (ret != ESP_OK)
        {
            break;
        }
    }
    xSemaphoreGive(At24_Mutex);
    return ret;
}

esp_err_t AT24_Read(uint16_t reg_addr, uint8_t *dat, uint16_t len)
{
    esp_err_t ret;
    xSemaphoreTake(At24_Mutex, -1);
    while (len)
    {
        ret = AT24CXX_ReadOneByte(reg_addr++, dat);
        if (ret != ESP_OK)
        {
            break;
        }

        dat++;
        len--;
    }
    xSemaphoreGive(At24_Mutex);
    return ret;
}

esp_err_t FM24C_WriteOneByte(uint16_t reg_addr, uint8_t dat)
{
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, DEV_ADD, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (reg_addr / 256), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (reg_addr % 256), ACK_CHECK_EN);

    i2c_master_write_byte(cmd, dat, ACK_CHECK_EN); //send data value

    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    vTaskDelay(10 / portTICK_RATE_MS);

    return ret;
}

// esp_err_t FM24C_ReadOneByte(uint16_t reg_addr, uint8_t *data)
// {

//     esp_err_t ret;
//     i2c_cmd_handle_t cmd = i2c_cmd_link_create();
//     i2c_master_start(cmd);
//     i2c_master_write_byte(cmd, (DEV_ADD + ((reg_addr / 256) << 1)), ACK_CHECK_EN);
//     i2c_master_write_byte(cmd, (reg_addr % 256), ACK_CHECK_EN);

//     i2c_master_start(cmd);
//     i2c_master_write_byte(cmd, (DEV_ADD + ((reg_addr / 256) << 1)) + 1, ACK_CHECK_EN);

//     i2c_master_read_byte(cmd, data, NACK_VAL); //只读1 byte 不需要应答

//     i2c_master_stop(cmd);
//     ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
//     i2c_cmd_link_delete(cmd);

//     vTaskDelay(20 / portTICK_RATE_MS);

//     return ret;
// }

esp_err_t FM24C_Write(uint16_t reg_addr, uint8_t *dat, uint16_t len)
{
    esp_err_t ret;
    xSemaphoreTake(At24_Mutex, -1);
    uint8_t t;
    for (t = 0; t < len; t++)
    {
        ret = FM24C_WriteOneByte(reg_addr + t, *dat);
        dat++;
        if (ret != ESP_OK)
        {
            break;
        }
    }
    xSemaphoreGive(At24_Mutex);
    return ret;
}

esp_err_t FM24C_Read(uint16_t reg_addr, uint8_t *dat, uint16_t len)
{
    xSemaphoreTake(At24_Mutex, -1);
    int ret, i;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, DEV_ADD, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (reg_addr / 256), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (reg_addr % 256), ACK_CHECK_EN);

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

    vTaskDelay(10 / portTICK_RATE_MS);
    xSemaphoreGive(At24_Mutex);
    return ret;
}

esp_err_t E2P_WriteOneByte(uint16_t reg_addr, uint8_t dat)
{
    int ret = ESP_OK;
    uint8_t check_temp[2];
    uint8_t data_buff[2];
    while (1)
    {
        data_buff[0] = dat;
        data_buff[1] = Get_Crc8(data_buff, 1);

        if (E2P_M)
        {
            FM24C_Write(reg_addr, data_buff, 2);
            FM24C_Read(reg_addr, check_temp, 2);
        }
        else
        {
            AT24_Write(reg_addr, data_buff, 2);
            AT24_Read(reg_addr, check_temp, 2);
        }

        if (memcmp(check_temp, data_buff, 2) == 0)
        {
            // ESP_LOGI(TAG, "reg_addr:%d,data_buff[0]=%d,data_buff[1]=%d", reg_addr, data_buff[0], data_buff[1]);
            // ESP_LOGI(TAG, "check_temp[0]=%d,check_temp[1]=%d", check_temp[0], check_temp[1]);
            break;
        }
        else
        {
            ESP_LOGE(TAG, "E2P_WriteOneByte err!!!");
            ret = ESP_FAIL;
        }
    }

    return ret;
}

uint8_t E2P_ReadOneByte(uint16_t reg_addr)
{
    uint8_t temp = 0;
    uint8_t temp_buf[2];
    bool ret = true;

    for (uint8_t i = 0; i < 10; i++)
    {
        if (E2P_M)
        {
            FM24C_Read(reg_addr, temp_buf, 2);
        }
        else
        {
            AT24_Read(reg_addr, temp_buf, 2);
            // ESP_LOGI(TAG, "reg_addr:%d,temp_buf[0]=%d,temp_buf[1]=%d", reg_addr, temp_buf[0], temp_buf[1]);
        }

        if (temp_buf[1] == Get_Crc8(temp_buf, 1))
        {
            temp = temp_buf[0];
            ret = true;
            break;
        }
        else
        {
            ret = false;
            ESP_LOGE(TAG, "temp_buf:%d,%d,CRC:%d ", temp_buf[0], temp_buf[1], Get_Crc8(temp_buf, 1));
            ESP_LOGE(TAG, "E2P_ReadOneByte err");
        }
    }
    if (ret == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        E2prom_empty_all(true);
        ESP_LOGI(TAG, "%d", __LINE__);
        esp_restart();
    }

    return temp;
}

//在AT24CXX里面的指定地址开始写入长度为Len的数据
//该函数用于写入16bit或者32bit的数据.
//WriteAddr  :开始写入的地址
//DataToWrite:数据数组首地址
//Len        :要写入数据的长度2,4
void E2P_WriteLenByte(uint16_t WriteAddr, uint32_t DataToWrite, uint8_t Len)
{
    uint8_t *check_temp;
    uint8_t t;
    uint8_t *data_temp;
    bool ret;

    data_temp = (uint8_t *)malloc(Len + 1);
    check_temp = (uint8_t *)malloc(Len + 1);

    while (1)
    {

        memset(data_temp, 0, Len + 1);
        memset(check_temp, 0, Len + 1);
        for (t = 0; t < Len; t++)
        {
            data_temp[t] = (DataToWrite >> (8 * t)) & 0xff;
        }
        data_temp[Len] = Get_Crc8(data_temp, Len);
        if (E2P_M)
        {
            FM24C_Write(WriteAddr, data_temp, Len + 1);
            FM24C_Read(WriteAddr, check_temp, Len + 1);
        }
        else
        {
            AT24_Write(WriteAddr, data_temp, Len + 1);
            AT24_Read(WriteAddr, check_temp, Len + 1);
        }

        if (memcmp(data_temp, check_temp, Len + 1) == 0)
        {
            ret = true;
            break;
        }
        else
        {
            ret = false;
            esp_log_buffer_hex(TAG, data_temp, Len + 1);
            esp_log_buffer_hex(TAG, check_temp, Len + 1);
            ESP_LOGE(TAG, "E2P_WriteLenByte err!!!");
        }
    }
    if (ret == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        E2prom_empty_all(true);
        ESP_LOGI(TAG, "%d", __LINE__);
        esp_restart();
    }

    free(data_temp);
    free(check_temp);
}

//在AT24CXX里面的指定地址开始读出长度为Len的数据
//该函数用于读出16bit或者32bit的数据.
//ReadAddr   :开始读出的地址
//返回值     :数据
//Len        :要读出数据的长度2,4
uint32_t E2P_ReadLenByte(uint16_t ReadAddr, uint8_t Len)
{
    uint8_t t;
    uint32_t temp = 0;
    uint8_t *data_temp;
    bool ret = true;
    data_temp = (uint8_t *)malloc(Len + 1);

    for (uint8_t i = 0; i < 10; i++)
    {
        if (E2P_M)
        {
            FM24C_Read(ReadAddr, data_temp, Len + 1);
        }
        else
        {
            AT24_Read(ReadAddr, data_temp, Len + 1);
        }

        if (data_temp[Len] == Get_Crc8(data_temp, Len)) //校验正确
        {
            for (t = 0; t < Len; t++)
            {
                temp += (data_temp[t] << (8 * t));
            }
            ret = true;
            break;
        }
        else
        {
            ret = false;
            esp_log_buffer_hex(TAG, data_temp, Len + 1);
            ESP_LOGE(TAG, "E2P_ReadLenByte err!!!");
        }
    }

    if (ret == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        E2prom_empty_all(true);
        ESP_LOGI(TAG, "%d", __LINE__);
        esp_restart();
    }

    free(data_temp);
    return temp;
}

//在AT24CXX里面的指定地址开始读出指定个数的数据
//ReadAddr :开始读出的地址 对24c08为0~1023
//pBuffer  :数据数组首地址
//NumToRead:要读出数据的个数
void E2P_Read(uint16_t ReadAddr, uint8_t *pBuffer, uint16_t NumToRead)
{
    uint8_t *check_buff;
    bool ret;
    check_buff = (uint8_t *)malloc(NumToRead + 1);

    for (uint8_t i = 0; i < 10; i++)
    {
        if (E2P_M)
        {
            FM24C_Read(ReadAddr, check_buff, NumToRead + 1);
        }
        else
        {
            AT24_Read(ReadAddr, check_buff, NumToRead + 1);
        }

        if (check_buff[NumToRead] == Get_Crc8(check_buff, NumToRead))
        {
            memcpy(pBuffer, check_buff, NumToRead);
            ret = true;
            break;
        }
        else
        {
            ret = false;
            ESP_LOGE(TAG, "E2P_Read err!!!");
            esp_log_buffer_hex(TAG, check_buff, NumToRead + 1);
        }
    }
    if (ret == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        E2prom_empty_all(false);
        ESP_LOGI(TAG, "%d", __LINE__);
        esp_restart();
    }

    free(check_buff);
}
//在AT24CXX里面的指定地址开始写入指定个数的数据
//WriteAddr :开始写入的地址 对24c08为0~1023
//pBuffer   :数据数组首地址
//NumToWrite:要写入数据的个数
void E2P_Write(uint16_t WriteAddr, uint8_t *pBuffer, uint16_t NumToWrite)
{
    uint8_t *check_buff;
    uint8_t *write_buff;
    check_buff = (uint8_t *)malloc(NumToWrite + 1);
    write_buff = (uint8_t *)malloc(NumToWrite + 1);

    while (1)
    {

        memset(check_buff, 0, NumToWrite + 1);
        memset(write_buff, 0, NumToWrite + 1);

        memcpy(write_buff, pBuffer, NumToWrite);
        write_buff[NumToWrite] = Get_Crc8(pBuffer, NumToWrite);
        if (E2P_M)
        {
            FM24C_Write(WriteAddr, write_buff, NumToWrite + 1);
            FM24C_Read(WriteAddr, check_buff, NumToWrite + 1);
        }
        else
        {
            AT24_Write(WriteAddr, write_buff, NumToWrite + 1);
            AT24_Read(WriteAddr, check_buff, NumToWrite + 1);
        }

        if (memcmp(write_buff, check_buff, NumToWrite + 1) == 0)
        {
            break;
        }
        else
        {
            ESP_LOGE(TAG, "E2P_Write err!!!");
        }
    }
    free(check_buff);
    free(write_buff);
}

esp_err_t E2P_Empty(uint16_t start_add, uint16_t end_add)
{
    if (end_add <= start_add)
    {
        return ESP_FAIL;
    }

    xSemaphoreTake(At24_Mutex, -1);

    esp_err_t ret = ESP_OK;
    uint16_t len;
    len = end_add - start_add;

    if (E2P_M)
    {
        while (len)
        {
            FM24C_WriteOneByte(start_add++, 0);
            len--;
        }
    }
    else
    {
        while (len)
        {
            AT24CXX_WriteOneByte(start_add++, 0);
            len--;
        }
    }

    xSemaphoreGive(At24_Mutex);
    return ret;
}

void E2prom_empty_all(bool flag)
{
    ESP_LOGI(TAG, "\nempty all set\n");

    if (flag)
    {
        // E2P_Empty(FN_SET_FLAG_ADD, E2P_SIZE - 1);
        // E2P_Empty(FN_SW_ON_ADD, E2P_USAGED);

        E2P_Empty(FN_SET_FLAG_ADD, WEB_PORT_ADD);
        E2P_Empty(FN_SW_ON_ADD, E2P_SIZE - 1);
    }
    else
    {
        E2P_Empty(0, E2P_SIZE);
    }
}
// static void E2prom_read_defaul(void)
// {
//     ESP_LOGI(TAG, "\nread defaul\n");

//     E2P_Read(SERISE_NUM_ADDR, (uint8_t *)SerialNum, SERISE_NUM_LEN);
//     E2P_Read(PRODUCT_ID_ADDR, (uint8_t *)ProductId, PRODUCT_ID_LEN);
//     E2P_Read(WEB_HOST_ADD, (uint8_t *)WEB_SERVER, WEB_HOST_LEN);
//     E2P_Read(WEB_PORT_ADD, (uint8_t *)WEB_PORT, 5);
//     E2P_Read(MQTT_HOST_ADD, (uint8_t *)MQTT_SERVER, WEB_HOST_LEN);
//     E2P_Read(MQTT_PORT_ADD, (uint8_t *)MQTT_PORT, 5);
// }
//清空并写入默认值
//flag =1 写入序列号相关
//flag =0 不写入序列号相关
void E2prom_set_defaul(bool flag)
{
    // E2prom_read_defaul();
    E2prom_empty_all(flag);
    //写入默认值
    ESP_LOGI(TAG, "set defaul\n");

    E2P_WriteLenByte(FN_DP_ADD, 60, 4);
    E2P_WriteOneByte(CG_DATA_LED_ADD, 1);
    esp_restart();
}

//检查AT24CXX是否正常,以及是否为新EEPROM
//这里用了24XX的最后一个地址(1023)来存储标志字.
//如果用其他24C系列,这个地址要修改

static bool E2P_Check(void)
{
    uint8_t Check_dat = 0x11;
    uint8_t temp;
    esp_err_t ret;
    i2c_cmd_handle_t cmd;
    uint8_t fail_num = 0;
    // temp = E2P_ReadOneByte(E2P_SIZE - 1);

    while (1)
    {
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, AT_DEV_ADD, ACK_CHECK_EN);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Use AT rom");
            E2P_M = 0;
            E2P_SIZE = AT_E2P_SIZE;
            DEV_ADD = AT_DEV_ADD;
            break;
        }

        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, FE_DEV_ADD, ACK_CHECK_EN);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Use FE rom");
            E2P_M = 1;
            E2P_SIZE = FE_E2P_SIZE;
            DEV_ADD = FE_DEV_ADD;
            break;
        }

        fail_num++;
        if (fail_num > 5)
        {
            ESP_LOGE(TAG, "E2p Check ERR");
            return false;
        }
    }

    // AT24_Read((E2P_SIZE - 100), &temp, 1);
    // ESP_LOGI(TAG, "temp:%2x", temp);

    if (E2P_M)
        FM24C_Read((E2P_SIZE - 1), &temp, 1);
    else
        AT24_Read((E2P_SIZE - 1), &temp, 1);

    ESP_LOGI(TAG, "temp:%2x", temp);

    if (temp == 0XFF)
    {
        if (E2P_M)
            FM24C_Read((E2P_SIZE - 10), &temp, 1);
        else
            AT24_Read((E2P_SIZE - 10), &temp, 1);

        ESP_LOGI(TAG, "temp:%2x", temp);
        if (temp == 0XFF)
        {
            ESP_LOGI(TAG, "\nnew eeprom\n");
            E2prom_set_defaul(false);
        }
    }

    if (temp == Check_dat)
    {
        ESP_LOGI(TAG, "eeprom check ok 1!\n");
        return true;
    }
    else //排除第一次初始化的情况
    {
        if (E2P_M)
        {
            FM24C_Write(E2P_SIZE - 1, &Check_dat, 1);
            FM24C_Read((E2P_SIZE - 1), &temp, 1);
            ESP_LOGI(TAG, "temp:%2x", temp);
        }
        else
        {
            AT24_Write(E2P_SIZE - 1, &Check_dat, 1);
            AT24_Read((E2P_SIZE - 1), &temp, 1);
            ESP_LOGI(TAG, "temp:%2x", temp);
        }

        if (temp == Check_dat)
        {
            ESP_LOGI(TAG, "eeprom check ok 2!\n");
            return true;
        }
    }
    ESP_LOGI(TAG, "eeprom check fail!\n");
    return false;
}

esp_err_t Nvs_Write_32(const char *Key, uint32_t dat)
{
    xSemaphoreTake(At24_Mutex, -1);
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_cache_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        vTaskDelay(1000 / portTICK_RATE_MS);
        esp_restart();
    }
    err = nvs_set_u32(nvs_cache_handle, Key, dat);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_set_i32 err\n");
        vTaskDelay(1000 / portTICK_RATE_MS);
        esp_restart();
    }

    err = nvs_commit(nvs_cache_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_commit err\n");
        vTaskDelay(1000 / portTICK_RATE_MS);
        esp_restart();
    }
    // Close
    nvs_close(nvs_cache_handle);
    xSemaphoreGive(At24_Mutex);
    return err;
}

uint32_t Nvs_Read_32(const char *Key)
{
    xSemaphoreTake(At24_Mutex, -1);
    uint32_t dat;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_cache_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        vTaskDelay(1000 / portTICK_RATE_MS);
        esp_restart();
    }
    err = nvs_get_u32(nvs_cache_handle, Key, &dat);
    if (err == ESP_OK)
    {
        // ESP_LOGI(TAG, "dat = %ld\n", dat);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "The value is not initialized yet!\n");
        dat = 0;
    }
    else
    {
        ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
        vTaskDelay(1000 / portTICK_RATE_MS);
        esp_restart();
        dat = 0;
    }
    // Close
    nvs_close(nvs_cache_handle);

    xSemaphoreGive(At24_Mutex);
    return dat;
}