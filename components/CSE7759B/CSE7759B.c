#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "Json_parse.h"
#include "CSE7759B.h"
#include "Uart0.h"
#include "Http.h"
#include "Cache_data.h"
#include "ServerTimer.h"
#include "Led.h"

#define TAG "CSE7759B"
TaskHandle_t CSE7759B_Handle = NULL;
SemaphoreHandle_t Energy_read_Binary = NULL;
SemaphoreHandle_t Ele_quan_Binary = NULL;

#define UART1_TXD (UART_PIN_NO_CHANGE)
#define UART1_RXD (GPIO_NUM_26)
#define UART1_RTS (UART_PIN_NO_CHANGE)
#define UART1_CTS (UART_PIN_NO_CHANGE)

#define BUF_SIZE 50
//static const char *TAG = "CSE7759b";

#define SAMPLE_RESISTANCE_MR 1 //使用的采样锰铜电阻mR值

#define UART_IND_HD 0
#define UART_IND_5A 1
#define UART_IND_VK 2
#define UART_IND_VT 5
#define UART_IND_IK 8
#define UART_IND_IT 11
#define UART_IND_PK 14
#define UART_IND_PT 17
#define UART_IND_FG 20
#define UART_IND_EN 21
#define UART_IND_SM 23

#define ARRAY_LEN 3 //平滑滤波buf长度
#define COUNT_NUM 1 //超时更新数据次数

//7759B电能计数脉冲溢出时的数据
#define ENERGY_FLOW_NUM 65536 //电量采集，电能溢出时的脉冲计数值

double sw_v_val;  //电压 V
double sw_c_val;  //电流 A
double sw_p_val;  //功率 W
double sw_pc_val; //用电量 W/h

typedef struct RuningInf_s
{
    unsigned short voltage;     //当前电压值，单位为0.1V
    unsigned short electricity; //当前电流值,单位为0.01A
    unsigned short power;       //当前电流值,单位为0.01A

    unsigned long energy;         //当前消耗电能值对应的脉冲个数
    unsigned short energyCurrent; //电能脉冲当前统计值
    unsigned short energyLast;    //电能脉冲上次统计值
    unsigned char energyFlowFlag; //电能脉冲溢出标致

    unsigned long energyUnit; //0.001度点对应的脉冲个数
} RuningInf_t;

RuningInf_t runingInf;

// bool CSE_Status = false;
// bool CSE_Energy_Status = false;

//获取电压、电流、功率的有限数据
unsigned long getVIPvalue(unsigned long *arr) //更新电压、电流、功率的列表
{
    int maxIndex = 0;
    int minIndex = 0;
    //unsigned long sum = 0;
    int j = 0;
    for (j = 1; j < ARRAY_LEN; j++)
    {
        if (arr[maxIndex] <= arr[j])
        { //避免所有数据一样时minIndex等于maxIndex
            maxIndex = j;
        }
        if (arr[minIndex] > arr[j])
        {
            minIndex = j;
        }
    }

    for (j = 0; j < ARRAY_LEN; j++)
    {
        if ((maxIndex == j) || (minIndex == j))
        {
            continue;
        }
        else
        {
            return arr[j];
        }
    }
    return arr[j];
}

int isUpdataNewData(unsigned long *arr, unsigned long dat) //检测电压电流功率是否需要更新
{
    if (arr[0] == dat)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

void updataVIPvalue(unsigned long *arr, unsigned long dat) //更新电压、电流、功率的列表
{
    int ii = ARRAY_LEN - 1;
    for (ii = ARRAY_LEN - 1; ii > 0; ii--)
    {
        arr[ii] = arr[ii - 1];
    }
    arr[0] = dat;
}

void resetVIPvalue(unsigned long *arr, unsigned long dat) //更新所有电压、电流、功率的列表
{
    int ii = ARRAY_LEN - 1;
    for (ii = ARRAY_LEN - 1; ii >= 0; ii--)
    {
        arr[ii] = dat;
    }
}

int DealUartInf(unsigned char *inDataBuffer, int recvlen)
{
    unsigned char startFlag = 0;

    unsigned long voltage_k = 0;
    unsigned long voltage_t = 0;
    unsigned long voltage = 0;
    static unsigned long voltage_a[ARRAY_LEN] = {0};
    static unsigned int voltageCnt = 0;

    unsigned long electricity_k = 0;
    unsigned long electricity_t = 0;
    unsigned long electricity = 0;
    static unsigned long electricity_a[ARRAY_LEN] = {0};
    static unsigned int electricityCnt = 0;

    unsigned long power_k = 0;
    unsigned long power_t = 0;
    unsigned long power = 0;
    static unsigned long power_a[ARRAY_LEN] = {0};
    static unsigned int powerCnt = 0;
    static unsigned long powerNewFlag = 1;

    unsigned int energy_cnt = 0;
    unsigned char energyFlowFlag = 0;
    // unsigned long energy = 0; //累计用电量

    startFlag = inDataBuffer[UART_IND_HD];

    voltage_k = ((inDataBuffer[UART_IND_VK] << 16) | (inDataBuffer[UART_IND_VK + 1] << 8) | (inDataBuffer[UART_IND_VK + 2]));     //电压系数
    voltage_t = ((inDataBuffer[UART_IND_VT] << 16) | (inDataBuffer[UART_IND_VT + 1] << 8) | (inDataBuffer[UART_IND_VT + 2]));     //电压周期
    electricity_k = ((inDataBuffer[UART_IND_IK] << 16) | (inDataBuffer[UART_IND_IK + 1] << 8) | (inDataBuffer[UART_IND_IK + 2])); //电流系数
    electricity_t = ((inDataBuffer[UART_IND_IT] << 16) | (inDataBuffer[UART_IND_IT + 1] << 8) | (inDataBuffer[UART_IND_IT + 2])); //电流周期
    power_k = ((inDataBuffer[UART_IND_PK] << 16) | (inDataBuffer[UART_IND_PK + 1] << 8) | (inDataBuffer[UART_IND_PK + 2]));       //功率系数
    power_t = ((inDataBuffer[UART_IND_PT] << 16) | (inDataBuffer[UART_IND_PT + 1] << 8) | (inDataBuffer[UART_IND_PT + 2]));       //功率周期
    // ESP_LOGI(TAG, "voltage_k=%ld,electricity_k=%ld,power_k=%ld", voltage_k, electricity_k, power_k);

    switch (startFlag)
    {
    case 0x55:
        if ((inDataBuffer[UART_IND_FG] & 0x40) == 0x40)
        {
            //获取当前电压标致，为1时说明电压检测OK
            if (isUpdataNewData(voltage_a, voltage_t))
            {
                updataVIPvalue(voltage_a, voltage_t);
                voltageCnt = 0;
            }
            else
            {
                voltageCnt++;
                if (voltageCnt >= COUNT_NUM)
                {
                    voltageCnt = 0;
                    updataVIPvalue(voltage_a, voltage_t);
                }
            }
            ESP_LOGD(TAG, "voltage:%ld,%ld,%ld\r\n", voltage_a[0], voltage_a[1], voltage_a[2]);
            voltage_t = getVIPvalue(voltage_a);

            if (voltage_t == 0)
            {
                voltage = 0;
            }
            else
            {
                voltage = voltage_k * 100 / voltage_t; //电压10mV值，避免溢出
                voltage = voltage * 10;                //电压mV值
            }
            ESP_LOGD(TAG, "11Vk = %ld,Vt = %ld,v = %ld\r\n", voltage_k, voltage_t, voltage);
            // mqtt_json_s.mqtt_Voltage = voltage / 1000;
        }
        else
        {
            ESP_LOGE(TAG, "%s(%d):V Flag Error\r\n", __func__, __LINE__);
        }

        if ((inDataBuffer[UART_IND_FG] & 0x20) == 0x20)
        {
            if (isUpdataNewData(electricity_a, electricity_t))
            {
                updataVIPvalue(electricity_a, electricity_t);
                electricityCnt = 0;
            }
            else
            {
                electricityCnt++;
                if (electricityCnt >= COUNT_NUM)
                {
                    electricityCnt = 0;
                    updataVIPvalue(electricity_a, electricity_t);
                }
            }
            ESP_LOGD(TAG, "electricity:%ld,%ld,%ld\r\n", electricity_a[0], electricity_a[1], electricity_a[2]);
            electricity_t = getVIPvalue(electricity_a);

            if (electricity_t == 0)
            {
                electricity = 0;
            }
            else
            {
                electricity = electricity_k * 100 / electricity_t; //电流10mA值，避免溢出
                electricity = electricity * 10;                    //电流mA值
#if (SAMPLE_RESISTANCE_MR == 1)
//由于使用1mR的电阻，电流和功率需要不除以2
#elif (SAMPLE_RESISTANCE_MR == 2)
                //由于使用2mR的电阻，电流和功率需要除以2
                electricity >>= 1;
#endif
            }
            ESP_LOGD(TAG, "11Ik = %ld,It = %ld,I = %ld\r\n", electricity_k, electricity_t, electricity);
            // mqtt_json_s.mqtt_Current = electricity / 1000.0;
        }
        else
        {
            ESP_LOGE(TAG, "%s(%d):I Flag Error\r\n", __func__, __LINE__);
        }

        if ((inDataBuffer[UART_IND_FG] & 0x10) == 0x10)
        {
            powerNewFlag = 0;
            //防止 0 做除数
            // if (power_k == 0 || power_t == 0)
            //     return -1;

            if (isUpdataNewData(power_a, power_t))
            {
                updataVIPvalue(power_a, power_t);
                powerCnt = 0;
            }
            else
            {
                powerCnt++;
                if (powerCnt >= COUNT_NUM)
                {
                    powerCnt = 0;
                    updataVIPvalue(power_a, power_t);
                }
            }
            ESP_LOGD(TAG, "power:%ld,%ld,%ld\r\n", power_a[0], power_a[1], power_a[2]);
            power_t = getVIPvalue(power_a);

            if (power_t == 0)
            {
                power = 0;
            }
            else
            {
                power = power_k * 100 / power_t; //功率10mw值，避免溢出
                power = power * 10;              //功率mw值
#if (SAMPLE_RESISTANCE_MR == 1)
//由于使用1mR的电阻，电流和功率需要不除以2
#elif (SAMPLE_RESISTANCE_MR == 2)
                //由于使用2mR的电阻，电流和功率需要除以2
                power >>= 1;
#endif
            }
            ESP_LOGD(TAG, "11Pk = %ld,Pt = %ld,P = %ld\r\n", power_k, power_t, power);
            // mqtt_json_s.mqtt_Power = power / 1000.0;
        }
        else if (powerNewFlag == 0)
        {

            if (isUpdataNewData(power_a, power_t))
            {
                unsigned long powerData = getVIPvalue(power_a);
                if (power_t > powerData)
                {
                    if ((power_t - powerData) > (powerData >> 2))
                    {
                        resetVIPvalue(power_a, power_t);
                    }
                }
            }
            ESP_LOGD(TAG, "power:%ld,%ld,%ld\r\n", power_a[0], power_a[1], power_a[2]);
            power_t = getVIPvalue(power_a);

            if (power_t == 0)
            {
                power = 0;
            }
            else
            {
                power = power_k * 100 / power_t; //功率10mw值，避免溢出
                power = power * 10;              //功率mw值
#if (SAMPLE_RESISTANCE_MR == 1)
//由于使用1mR的电阻，电流和功率需要不除以2
#elif (SAMPLE_RESISTANCE_MR == 2)
                //由于使用2mR的电阻，电流和功率需要除以2
                power >>= 1;
#endif
            }
            ESP_LOGD(TAG, "22Pk = %ld,Pt = %ld,P = %ld\r\n", power_k, power_t, power);
            // mqtt_json_s.mqtt_Power = power / 1000.0;
        }

        energyFlowFlag = (inDataBuffer[UART_IND_FG] >> 7);                                              //获取当前电能计数溢出标致
        runingInf.energyCurrent = ((inDataBuffer[UART_IND_EN] << 8) | (inDataBuffer[UART_IND_EN + 1])); //更新当前的脉冲计数值
        if (runingInf.energyFlowFlag != energyFlowFlag)
        { //每次计数溢出时更新当前脉冲计数值
            runingInf.energyFlowFlag = energyFlowFlag;
            if (runingInf.energyCurrent > runingInf.energyLast)
            {
                runingInf.energyCurrent = 0;
            }
            energy_cnt = runingInf.energyCurrent + ENERGY_FLOW_NUM - runingInf.energyLast;
        }
        else
        {
            energy_cnt = runingInf.energyCurrent - runingInf.energyLast;
        }
        runingInf.energyLast = runingInf.energyCurrent;
        runingInf.energy += (energy_cnt * 10); //电能个数累加时扩大10倍，计算电能是除数扩大10倍，保证计算精度

        runingInf.energyUnit = 0xD693A400 >> 1;
        // if (power_k == 0)
        //     return -1;
        runingInf.energyUnit /= (power_k >> 1); //1mR采样电阻0.001度电对应的脉冲个数

#if (SAMPLE_RESISTANCE_MR == 1)
//1mR锰铜电阻对应的脉冲个数
#elif (SAMPLE_RESISTANCE_MR == 2)
        //2mR锰铜电阻对应的脉冲个数
        runingInf.energyUnit = (runingInf.energyUnit << 1); //2mR采样电阻0.001度电对应的脉冲个数
#endif
        runingInf.energyUnit = runingInf.energyUnit * 10; //0.001度电对应的脉冲个数(计算个数时放大了10倍，所以在这里也要放大10倍)

        //电能使用量
        // energy = runingInf.energy / runingInf.energyUnit; //单位是0.001度
        // mqtt_json_s.mqtt_Energy = energy / 1000.0;        //单位是度
        // ESP_LOGD(TAG, "energy=%ld\n", energy);
        break;

    case 0xAA:
        //芯片未校准
        // CSE_Status = false;
        ESP_LOGE(TAG, "CSE7759B not check\r\n");
        return -1;
        break;

    default:
        if ((startFlag & 0xF1) == 0xF1)
        {
            //存储区异常，芯片坏了
            // CSE_Status = false;
            ESP_LOGE(TAG, "CSE7759B broken\r\n");
            return -1;
        }

        if ((startFlag & 0xF2) == 0xF2)
        {
            //功率异常,电流设置为0
            runingInf.power = 0; //获取到的功率是以0.1W为单位
            power = 0;
            runingInf.electricity = 0; //获取到的电流以0.01A为单位
            electricity = 0;
            ESP_LOGD(TAG, "Power Error\r\n");
        }
        else
        {
            if ((inDataBuffer[UART_IND_FG] & 0x10) == 0x10)
            {
                powerNewFlag = 0;

                if (isUpdataNewData(power_a, power_t))
                {
                    updataVIPvalue(power_a, power_t);
                    powerCnt = 0;
                }
                else
                {
                    powerCnt++;
                    if (powerCnt >= COUNT_NUM)
                    {
                        powerCnt = 0;
                        updataVIPvalue(power_a, power_t);
                    }
                }
                //ESP_LOGD(TAG,"power:%ld,%ld,%ld\r\n",power_a[0],power_a[1],power_a[2]);
                power_t = getVIPvalue(power_a);

                if (power_t == 0)
                {
                    power = 0;
                }
                else
                {
                    power = power_k * 100 / power_t; //功率10mw值，避免溢出
                    power = power * 10;              //功率mw值
#if (SAMPLE_RESISTANCE_MR == 1)
//由于使用1mR的电阻，电流和功率需要不除以2
#elif (SAMPLE_RESISTANCE_MR == 2)
                    //由于使用2mR的电阻，电流和功率需要除以2
                    power >>= 1;
#endif
                }
                ESP_LOGD(TAG, "33Pk = %ld,Pt = %ld,P = %ld\r\n", power_k, power_t, power);
                // mqtt_json_s.mqtt_Power = power / 1000.0;
            }
            else if (powerNewFlag == 0)
            {

                if (isUpdataNewData(power_a, power_t))
                {
                    unsigned long powerData = getVIPvalue(power_a);
                    if (power_t > powerData)
                    {
                        if ((power_t - powerData) > (powerData >> 2))
                        {
                            resetVIPvalue(power_a, power_t);
                        }
                    }
                }
                //ESP_LOGD(TAG,"power:%ld,%ld,%ld\r\n",power_a[0],power_a[1],power_a[2]);
                power_t = getVIPvalue(power_a);

                if (power_t == 0)
                {
                    power = 0;
                }
                else
                {
                    power = power_k * 100 / power_t; //功率10mw值，避免溢出
                    power = power * 10;              //功率mw值
#if (SAMPLE_RESISTANCE_MR == 1)
//由于使用1mR的电阻，电流和功率需要不除以2
#elif (SAMPLE_RESISTANCE_MR == 2)
                    //由于使用2mR的电阻，电流和功率需要除以2
                    power >>= 1;
#endif
                }
                ESP_LOGD(TAG, "44Pk = %ld,Pt = %ld,P = %ld\r\n", power_k, power_t, power);
                // mqtt_json_s.mqtt_Power = power / 1000.0;
            }
        }

        if ((startFlag & 0xF4) == 0xF4)
        {                              //电流异常
            runingInf.electricity = 0; //获取到的电流以0.01A为单位
            electricity = 0;
        }
        else if ((startFlag & 0xF2) != 0xF2) //如果功率异常，但电流正常，则判断电流为0
        {
            if ((inDataBuffer[UART_IND_FG] & 0x20) == 0x20)
            {

                if (isUpdataNewData(electricity_a, electricity_t))
                {
                    updataVIPvalue(electricity_a, electricity_t);
                    electricityCnt = 0;
                }
                else
                {
                    electricityCnt++;
                    if (electricityCnt >= COUNT_NUM)
                    {
                        electricityCnt = 0;
                        updataVIPvalue(electricity_a, electricity_t);
                    }
                }
                //ESP_LOGD(TAG,"electricity:%ld,%ld,%ld\r\n",electricity_a[0],electricity_a[1],electricity_a[2]);
                electricity_t = getVIPvalue(electricity_a);

                if (electricity_t == 0)
                {
                    electricity = 0;
                }
                else
                {
                    electricity = electricity_k * 100 / electricity_t; //电流10mA值，避免溢出
                    electricity = electricity * 10;                    //电流mA值
#if (SAMPLE_RESISTANCE_MR == 1)
//由于使用1mR的电阻，电流和功率需要不除以2
#elif (SAMPLE_RESISTANCE_MR == 2)
                    //由于使用2mR的电阻，电流和功率需要除以2
                    electricity >>= 1;
#endif
                }
                ESP_LOGD(TAG, "22Ik = %ld,It = %ld,I = %ld\r\n", electricity_k, electricity_t, electricity);
                // mqtt_json_s.mqtt_Current = electricity / 1000.0;
            }
            else
            {
                ESP_LOGD(TAG, "%s(%d):I Flag Error\r\n", __func__, __LINE__);
            }
        }

        if ((startFlag & 0xF8) == 0xF8)
        {                          //电压异常
            runingInf.voltage = 0; //获取到的电压是以0.1V为单位
            voltage = 0;
        }
        else
        {
            if ((inDataBuffer[UART_IND_FG] & 0x40) == 0x40)
            { //获取当前电压标致，为1时说明电压检测OK

                if (isUpdataNewData(voltage_a, voltage_t))
                {
                    updataVIPvalue(voltage_a, voltage_t);
                    voltageCnt = 0;
                }
                else
                {
                    voltageCnt++;
                    if (voltageCnt >= COUNT_NUM)
                    {
                        voltageCnt = 0;
                        updataVIPvalue(voltage_a, voltage_t);
                    }
                }
                //ESP_LOGD(TAG,"voltage:%ld,%ld,%ld\r\n",voltage_a[0],voltage_a[1],voltage_a[2]);
                voltage_t = getVIPvalue(voltage_a);

                if (voltage_t == 0)
                {
                    voltage = 0;
                }
                else
                {
                    voltage = voltage_k * 100 / voltage_t; //电压10mV值，避免溢出
                    voltage = voltage * 10;                //电压mV值
                }
                ESP_LOGD(TAG, "22Vk = %ld,Vt = %ld,v = %ld\r\n", voltage_k, voltage_t, voltage);
                // mqtt_json_s.mqtt_Voltage = voltage / 1000;
            }
            else
            {
                ESP_LOGE(TAG, "%s(%d):V Flag Error\r\n", __func__, __LINE__);
            }
        }

        break;
    }
    ESP_LOGI(TAG, "0x%x:V = %ld;I = %ld;P = %ld;\r\n", startFlag, voltage, electricity, power);
    sw_v_val = voltage / 1000;
    sw_c_val = electricity / 1000.0;
    sw_p_val = power / 1000.0;
    // CSE_Status = true;
    return 1;
}

//串口读取原始数据
int8_t CSE7759B_Read(void)
{
    uint8_t data_u1[BUF_SIZE] = {0};
    uint8_t data_7759b[24] = {0};
    uint16_t checksum = 0;
    int len_7759_start = 0;

    sw_uart2(uart2_cse);
    int len1 = uart_read_bytes(UART_NUM_2, data_u1, BUF_SIZE, 150 / portTICK_RATE_MS);
    xSemaphoreGive(xMutex_uart2_sw);

    if (len1 != 0) //读取到数据
    {
        // ESP_LOGD(TAG,"CSE7759B_Read len1 = %d\n", len1);
        //查找数据头
        for (int i = 0; i < len1; i++)
        {
            if ((data_u1[i] == 0x5a) && (((data_u1[i - 1] & 0xf0) == 0xf0) || (data_u1[i - 1] == 0xaa) || (data_u1[i - 1] == 0x55)))
            {
                len_7759_start = i - 1;
                //取得24byte计量数据
                for (int i = 0; i < 24; i++)
                {
                    data_7759b[i] = data_u1[len_7759_start + i];
                }

                // ESP_LOGI(TAG, "7759b=");
                for (int i = 0; i < 24; i++)
                {
                    // ESP_LOGI(TAG, "0x%02x ", data_7759b[i]);
                    if (i >= 2 && i <= 22)
                    {
                        checksum += data_7759b[i];
                    }
                    else if (i == 23)
                    {
                        checksum = checksum & 0xff;
                        if (checksum == data_7759b[23])
                        {
                            ESP_LOGI(TAG, "checksum = 0x%02x ,7759b checksum ok!", checksum);
                            if (DealUartInf(data_7759b, 24)) //处理7759B数据
                            {
                                return 1;
                            }
                            ESP_LOGE(TAG, "Chip CSE7759B ERR!");
                            return 0;
                        }
                        else
                        {
                            // CSE_Status = false;
                            ESP_LOGE(TAG, "checksum = 0x%02x ,7759b checksum fail!", checksum);
                            return 0;
                        }
                    }
                }
            }
        }
        ESP_LOGE(TAG, "CSE7759B Date Err！\n");
        // CSE_Status = false;
        return 0;
    }
    else
    {
        ESP_LOGE(TAG, "No CSE7759B Date！\n");
        // CSE_Status = false;
        return 0;
    }
}

void Creat_Ele_quan_Json(void)
{
}

//读取，构建电能信息
void Energy_Read_Task(void *pvParameters)
{
    char *filed_buff;
    char *OutBuffer;
    // uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;

    CSE7759B_Read();
    vTaskDelay(2000 / portTICK_PERIOD_MS); //上电初始化
    CSE7759B_Read();
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        if (mqtt_json_s.mqtt_switch_status)
        {
            while (CSE7759B_Read() != 1)
            {
                Led_Status = LED_STA_HEARD_ERR;
                vTaskDelay(2000 / portTICK_PERIOD_MS); //
            }
            filed_buff = (char *)malloc(9);

            pJsonRoot = cJSON_CreateObject();
            cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
            // cJSON_AddItemToObject(pJsonRoot, "field1", cJSON_CreateNumber(mqtt_json_s.mqtt_switch_status));
            snprintf(filed_buff, 9, "field%d", sw_v_f_num);
            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(sw_v_val));
            snprintf(filed_buff, 9, "field%d", sw_c_f_num);
            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(sw_c_val));
            snprintf(filed_buff, 9, "field%d", sw_p_f_num);
            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(sw_p_val));

            OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
            cJSON_Delete(pJsonRoot);                       //delete cjson root
            len = strlen(OutBuffer);
            printf("len:%d\n%s\n", len, OutBuffer);
            // SaveBuffer = (uint8_t *)malloc(len);
            // memcpy(SaveBuffer, OutBuffer, len);
            xSemaphoreTake(Cache_muxtex, -1);
            DataSave((uint8_t *)OutBuffer, len);
            xSemaphoreGive(Cache_muxtex);
            free(OutBuffer);
            free(filed_buff);
            // free(SaveBuffer);
        }

        // if (Binary_mqtt != NULL)
        // {
        //     xTaskNotifyGive(Binary_mqtt);
        // }
    }
}
//读取，构建累积用电量
void Ele_quan_Task(void *pvParameters)
{
    char *filed_buff;
    char *OutBuffer;
    // uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;

    vTaskDelay(2000 / portTICK_PERIOD_MS); //上电初始化
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        if (runingInf.energyUnit == 0)
            sw_pc_val = 0;
        else
            sw_pc_val = runingInf.energy / runingInf.energyUnit; //单位是 W/H
        // ESP_LOGI(TAG, "energy=%f\n", mqtt_json_s.mqtt_Energy);
        runingInf.energy = 0; //清除本次统计

        filed_buff = (char *)malloc(9);

        pJsonRoot = cJSON_CreateObject();
        cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
        snprintf(filed_buff, 9, "field%d", sw_pc_f_num);
        cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(sw_pc_val));
        OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
        cJSON_Delete(pJsonRoot);                       //delete cjson root
        len = strlen(OutBuffer);
        printf("len:%d\n%s\n", len, OutBuffer);
        // SaveBuffer = (uint8_t *)malloc(len);
        // memcpy(SaveBuffer, OutBuffer, len);
        xSemaphoreTake(Cache_muxtex, -1);
        DataSave((uint8_t *)OutBuffer, len);
        xSemaphoreGive(Cache_muxtex);
        free(OutBuffer);
        // free(SaveBuffer);
        free(filed_buff);
    }
}

void CSE7759B_Init(void)
{
    xTaskCreate(Energy_Read_Task, "Energy_Read_Task", 4096, NULL, 5, &Binary_energy);
    xTaskCreate(Ele_quan_Task, "Ele_quan_Task", 4096, NULL, 5, &Binary_ele_quan);
}
