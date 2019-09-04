#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "RS485_Read.h"

#define TAG "458"

#define UART2_TXD (GPIO_NUM_17)
#define UART2_RXD (GPIO_NUM_16)
#define UART2_RTS (UART_PIN_NO_CHANGE)
#define UART2_CTS (UART_PIN_NO_CHANGE)

#define RS485RD 21

#define BUF_SIZE 128

float ext_tem = 0.0, ext_hum = 0.0;

char wind_modbus_send_data[] = {0x20, 0x04, 0x00, 0x06, 0x00, 0x01, 0xd7, 0x7a};    //发送查询风速指令
char tem_hum_modbus_send_data[] = {0xC1, 0x03, 0x00, 0x00, 0x00, 0x02, 0xD5, 0x0B}; //发送 查询外接空气温湿度探头

/*******************************************************************************
// CRC16_Modbus Check
*******************************************************************************/
static uint16_t CRC16_ModBus(uint8_t *buf, uint8_t buf_len)
{
    uint8_t i, j;
    uint8_t c_val;
    uint16_t crc16_val = 0xffff;
    uint16_t crc16_poly = 0xa001; //0x8005

    for (i = 0; i < buf_len; i++)
    {
        crc16_val = *buf ^ crc16_val;

        for (j = 0; j < 8; j++)
        {
            c_val = crc16_val & 0x01;

            crc16_val = crc16_val >> 1;

            if (c_val)
            {
                crc16_val = crc16_val ^ crc16_poly;
            }
        }
        buf++;
    }
    // printf("crc16_val=%x\n", ((crc16_val & 0x00ff) << 8) | ((crc16_val & 0xff00) >> 8));
    return ((crc16_val & 0x00ff) << 8) | ((crc16_val & 0xff00) >> 8);
}

void RS485_Read(void)
{
    uint8_t data_u2[BUF_SIZE];
    // gpio_set_level(RS485RD, 1); //RS485输出模式
    uart_write_bytes(UART_NUM_2, tem_hum_modbus_send_data, 8);
    // vTaskDelay(100 / portTICK_PERIOD_MS);
    // gpio_set_level(RS485RD, 0); //RS485输入模式
    int len2 = uart_read_bytes(UART_NUM_2, data_u2, BUF_SIZE, 100 / portTICK_RATE_MS);

    if (len2 != 0)
    {
        // printf("UART2 len2: %d  recv:", len2);
        // for (int i = 0; i < len2; i++)
        // {
        //     printf("%x ", data_u2[i]);
        // }
        // printf("\r\n");

        //crc_check=CRC16_ModBus(data_u2,5);
        //printf("crc-check=%d\n",crc_check);
        //printf("crc-check2=%d\n",(data_u2[5]*256+data_u2[6]));
        //printf("u25=%x,u26=%x\n",data_u2[5],data_u2[6]);
        if ((data_u2[7] * 256 + data_u2[8]) == CRC16_ModBus(data_u2, (len2 - 2)))
        {
            if ((data_u2[0] == 0xC1) && (data_u2[1] == 0x03))
            {
                ext_tem = ((data_u2[3] << 8) + data_u2[4]) * 0.1;
                ext_hum = ((data_u2[5] << 8) + data_u2[6]) * 0.1;
                ESP_LOGI(TAG, "ext_tem=%f   ext_hum=%f\n", ext_tem, ext_hum);
            }
            else
            {
                ESP_LOGE(TAG, "485 add or cmd error\n");
            }
        }
        else
        {
            ESP_LOGE(TAG, "485 CRC error\n");
        }
        len2 = 0;
    }
    else
    {
        ESP_LOGE(TAG, "RS485 NO ARK !!! \n");
    }
}

void RS485_Init(void)
{
    /**********************uart init**********************************************/
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, UART2_TXD, UART2_RXD, UART2_RTS, UART2_CTS);
    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0);

    /******************************gpio init*******************************************/
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO16
    io_conf.pin_bit_mask = (1 << RS485RD);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 1;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
}

// /*******************************************************************************
// //check UART1 recive buffer
// *******************************************************************************/
// bool Uart_recv_buf_check(char *cmd_buf, char *recv_buf, uint8_t buf_len)
// {
//     uint8_t i;

//     for (i = 0; i < buf_len; i++)
//     {
//         if (cmd_buf[i] != recv_buf[i])
//             return 1;
//     }
//     return 0;
// }

// /*******************************************************************************
// //UART1 To RS485 temp sensor value
// *******************************************************************************/

// void Rs485_t_Sensor_val(float *temp_val)
// {
//     uint16_t th_crc16_val;
//     unsigned char rs485t_cmd[] = {0xC2, 0x03, 0x00, 0x00, 0x00, 0x01, 0x95, 0x39};

//     *temp_val = ERROR_CODE;

//     Uart1_Rev_Init(); //clear the recive buffer

//     osi_Uart1_Write(rs485t_cmd, sizeof(rs485t_cmd)); //Send the command

//     MAP_UtilsDelay(2000000); //delay about 150ms

// #ifdef DEBUG
//     UART_PRINT("rt:%02x,%02x,%02x,%02x,%02x,%02x,%02x\r\n", Uart1_Buf[0], Uart1_Buf[1], Uart1_Buf[2], Uart1_Buf[3], Uart1_Buf[4], Uart1_Buf[5], Uart1_Buf[6]);
// #endif

//     th_crc16_val = CRC16_ModBus((uint8_t *)Uart1_Buf, 5); //Check the data
//     if (((th_crc16_val / 256) == Uart1_Buf[5]) && ((th_crc16_val % 256) == Uart1_Buf[6]))
//     {
//         *temp_val = 256 * Uart1_Buf[3] + Uart1_Buf[4];

//         if (*temp_val != 0x8000) //sensor error
//         {
//             if (*temp_val <= 0x7fff) //"+"
//             {
//                 *temp_val = *temp_val / 10;
//             }
//             else //"-"
//             {
//                 *temp_val = 0xffff - *temp_val;

//                 *temp_val = -*temp_val / 10;
//             }
//         }
//     }
// }

// /*******************************************************************************
// //UART1 To RS485 temp/humi sensor task
// *******************************************************************************/
// void Rs485_t_Sensor(void *pvParameters)
// {
//     float rs485_t_val;
//     SensorMessage rs485tMsg;

//     for (;;)
//     {
//         osi_SyncObjWait(&Rs485t_Binary, OSI_WAIT_FOREVER); //waite UART1 interrupt message

//         rs485_init(UART1_SLOW_BAUD_RATE); //rs485 init

//         Rs485_t_Sensor_val(&rs485_t_val);

//         rs485_uinit(); //rs485 uinit

//         if (rs485_t_val != ERROR_CODE)
//         {
//             rs485tMsg.sensornum = r1_t_f_num; //Message Number

//             rs485tMsg.sensorval = rs485_t_val; //Message Value

//             osi_MsgQWrite(&Data_Queue, &rs485tMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_E_T_Status(1); //sensor exist status

//             if (lcd_display_num == E_T_SENSOR_NUM)
//             {
//                 clear_sensor_Status(); //clear all sensors data status

//                 Display_E_T_dataStatus(1);

//                 HT9B95A_Display_Temp_Val(rs485_t_val, C_F_Temp); //Display the temprature value

//                 HT9B95A_clear_down_area(); //clear down display area
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }
//         else
//         {
//             osi_TaskDisable(); //disable the tasks

//             Display_E_T_Status(0); //sensor inexistence status

//             if (lcd_display_num == E_T_SENSOR_NUM)
//             {
//                 HT9B95A_clear_up_area();

//                 HT9B95A_clear_down_area();
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }
//     }
// }

// /*******************************************************************************
// //UART1 To RS485 temp/humi sensor value
// *******************************************************************************/
// void Rs485_th_Sensor_val(float *temp_val, float *humi_val)
// {
//     uint16_t th_crc16_val;
//     unsigned char rs485th_cmd[] = {0xC1, 0x03, 0x00, 0x00, 0x00, 0x02, 0xD5, 0x0B}; //{0x01,0x03,0x00,0x00,0x00,0x02,0xC4,0x0B};

//     *temp_val = ERROR_CODE;

//     *humi_val = ERROR_CODE;

//     Uart1_Rev_Init(); //clear the recive buffer

//     osi_Uart1_Write(rs485th_cmd, sizeof(rs485th_cmd)); //Send the command

//     MAP_UtilsDelay(2000000); //delay about 150ms

// #ifdef DEBUG
//     UART_PRINT("th:%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\r\n", Uart1_Buf[0], Uart1_Buf[1], Uart1_Buf[2], Uart1_Buf[3], Uart1_Buf[4], Uart1_Buf[5], Uart1_Buf[6], Uart1_Buf[7], Uart1_Buf[8]);
// #endif

//     if (Uart_recv_buf_check((char *)rs485th_cmd, Uart1_Buf, sizeof(rs485th_cmd)))
//         th_crc16_val = CRC16_ModBus((uint8_t *)Uart1_Buf, 7); //Check the data
//     if (((th_crc16_val / 256) == Uart1_Buf[7]) && ((th_crc16_val % 256) == Uart1_Buf[8]))
//     {
//         *temp_val = 256 * Uart1_Buf[3] + Uart1_Buf[4];

//         if (*temp_val != 0x8000) //sensor error
//         {
//             if (*temp_val <= 0x7fff) //"+"
//             {
//                 *temp_val = *temp_val / 10;
//             }
//             else //"-"
//             {
//                 *temp_val = 0xffff - *temp_val;

//                 *temp_val = -*temp_val / 10;
//             }
//         }

//         *humi_val = 256 * Uart1_Buf[5] + Uart1_Buf[6];
//         if (*humi_val != 0x8000) //sensor error
//         {
//             *humi_val = *humi_val / 10;
//         }
//     }
// }

// /*******************************************************************************
// //UART1 To RS485 temp/humi sensor task
// *******************************************************************************/
// void Rs485_th_Sensor(void *pvParameters)
// {
//     float rs485_t_val, rs485_h_val;
//     SensorMessage rs485thMsg;

//     for (;;)
//     {
//         osi_SyncObjWait(&Rs485th_Binary, OSI_WAIT_FOREVER); //waite UART1 interrupt message

//         rs485_init(UART1_SLOW_BAUD_RATE); //rs485 init

//         Rs485_th_Sensor_val(&rs485_t_val, &rs485_h_val);

//         rs485_uinit(); //rs485 uinit

//         if ((rs485_t_val != ERROR_CODE) && (rs485_h_val != ERROR_CODE))
//         {
//             rs485thMsg.sensornum = r1_th_t_f_num; //Message Number

//             rs485thMsg.sensorval = rs485_t_val; //Message Value

//             osi_MsgQWrite(&Data_Queue, &rs485thMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             rs485thMsg.sensornum = r1_th_h_f_num; //Message Number

//             rs485thMsg.sensorval = rs485_h_val; //Message Value

//             osi_MsgQWrite(&Data_Queue, &rs485thMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_E_TH_Status(1); //sensor exist status

//             if (lcd_display_num == E_TH_SENSOR_NUM)
//             {
//                 clear_sensor_Status(); //clear all sensors data status

//                 Display_TH_dataStatus(1);

//                 HT9B95A_Display_Temp_Val(rs485_t_val, C_F_Temp); //Display the temprature value

//                 HT9B95A_Display_Humi_Val((uint8_t)rs485_h_val); //Display the Humility value
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }
//         else
//         {
//             osi_TaskDisable(); //disable the tasks

//             Display_E_TH_Status(0); //sensor inexistence status

//             if (lcd_display_num == E_TH_SENSOR_NUM)
//             {
//                 HT9B95A_clear_up_area();

//                 HT9B95A_clear_down_area();
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }
//     }
// }

// /*******************************************************************************
// //UART1 To RS485 temp/humi sensor value
// *******************************************************************************/
// void Rs485_s_th_Sensor_val(float *temp_val, float *humi_val)
// {
//     uint16_t sth_crc16_val;
//     unsigned char rs485_sth_cmd[] = {0xFE, 0x03, 0x00, 0x00, 0x00, 0x02, 0xD0, 0x04}; //read soil temp/humi value

//     Uart1_Rev_Init(); //clear the recive buffer

//     osi_Uart1_Write(rs485_sth_cmd, sizeof(rs485_sth_cmd)); //Send the command

//     MAP_UtilsDelay(2000000); //delay about 150ms

// #ifdef DEBUG
//     UART_PRINT("sth:%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\r\n", Uart1_Buf[0], Uart1_Buf[1], Uart1_Buf[2], Uart1_Buf[3], Uart1_Buf[4], Uart1_Buf[5], Uart1_Buf[6], Uart1_Buf[7], Uart1_Buf[8]);
// #endif

//     if (Uart_recv_buf_check((char *)rs485_sth_cmd, Uart1_Buf, sizeof(rs485_sth_cmd)))
//         sth_crc16_val = CRC16_ModBus((uint8_t *)Uart1_Buf, 7); //Check the recive data
//     if (((sth_crc16_val / 256) == Uart1_Buf[7]) && ((sth_crc16_val % 256) == Uart1_Buf[8]))
//     {
//         *temp_val = 256 * Uart1_Buf[5] + Uart1_Buf[6];

//         if (*temp_val <= 0x7fff) //"+"
//         {
//             *temp_val = *temp_val / 10;
//         }
//         else //"-"
//         {
//             *temp_val = 0xffff - *temp_val;

//             *temp_val = -*temp_val / 10;
//         }

//         *humi_val = 256 * Uart1_Buf[3] + Uart1_Buf[4];

//         *humi_val = *humi_val / 10; //humi Value
//     }
//     else
//     {
//         *temp_val = ERROR_CODE;

//         *humi_val = ERROR_CODE;
//     }
// }

// /*******************************************************************************
// //UART1 To RS485 temp/humi sensor task
// *******************************************************************************/
// void Rs485_s_th_Sensor(void *pvParameters)
// {
//     float rs485_st_val, rs485_sh_val;
//     SensorMessage rs485sthMsg;

//     for (;;)
//     {
//         osi_SyncObjWait(&Rs485sth_Binary, OSI_WAIT_FOREVER); //waite UART1 interrupt message

//         rs485_init(UART1_SLOW_BAUD_RATE); //rs485 init

//         Rs485_s_th_Sensor_val(&rs485_st_val, &rs485_sh_val);

//         rs485_uinit(); //rs485 uinit

//         if ((rs485_st_val != ERROR_CODE) && (rs485_sh_val != ERROR_CODE))
//         {
//             rs485sthMsg.sensornum = r1_sth_t_f_num; //Message Number

//             rs485sthMsg.sensorval = rs485_st_val; //Message Value

//             osi_MsgQWrite(&Data_Queue, &rs485sthMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             rs485sthMsg.sensornum = r1_sth_h_f_num; //Message Number

//             rs485sthMsg.sensorval = rs485_sh_val; //Message Value

//             osi_MsgQWrite(&Data_Queue, &rs485sthMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_S_TH_Status(1); //sensor exist status

//             if (lcd_display_num == S_TH_SENSOR_NUM)
//             {
//                 clear_sensor_Status(); //clear all sensors data status

//                 Display_S_TH_dataStatus(1);

//                 HT9B95A_Display_Temp_Val(rs485_st_val, C_F_Temp); //Display the temprature value

//                 HT9B95A_Display_Humi_Val((uint8_t)rs485_sh_val); //Display the Humility value
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }
//         else
//         {
//             osi_TaskDisable(); //disable the tasks

//             Display_S_TH_Status(0); //sensor inexistence status

//             if (lcd_display_num == S_TH_SENSOR_NUM)
//             {
//                 HT9B95A_clear_up_area();

//                 HT9B95A_clear_down_area();
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }
//     }
// }

// /*******************************************************************************
// //UART1 To RS485 wind speed sensor value
// *******************************************************************************/
// void Rs485_WindSpeed_Sensor_val(float *speed_val)
// {
//     uint16_t i;
//     uint16_t th_crc16_val;
//     unsigned char rs485th_cmd[] = {0x20, 0x04, 0x00, 0x06, 0x00, 0x01, 0xD7, 0x7A};
//     char rev_buf[7];

//     *speed_val = ERROR_CODE;

//     Uart1_Rev_Init(); //clear the recive buffer

//     osi_Uart1_Write(rs485th_cmd, sizeof(rs485th_cmd)); //Send the command

//     MAP_UtilsDelay(2000000); //delay about 150ms

//     for (i = 0; i < UART1_BUF_SIZE - 7; i++)
//     {
//         if (Uart1_Buf[i] == 0x20)
//         {
//             mem_copy(rev_buf, Uart1_Buf + i, 7);
// #ifdef DEBUG
//             UART_PRINT("ws:%02x,%02x,%02x,%02x,%02x,%02x,%02x\r\n", rev_buf[0], rev_buf[1], rev_buf[2], rev_buf[3], rev_buf[4], rev_buf[5], rev_buf[6]);
// #endif
//             th_crc16_val = CRC16_ModBus((uint8_t *)rev_buf, 5); //Check the data
//             if (((th_crc16_val / 256) == rev_buf[5]) && ((th_crc16_val % 256) == rev_buf[6]))
//             {
//                 *speed_val = 256 * rev_buf[3] + rev_buf[4];

//                 *speed_val = *speed_val / 10;

//                 break;
//             }
//         }
//     }
// }

// /*******************************************************************************
// //UART1 To RS485 wind speed sensor task
// *******************************************************************************/
// void Rs485_WindSpeed_Sensor(void *pvParameters)
// {
//     float WindSpeed_val;
//     SensorMessage rs485thMsg;

//     for (;;)
//     {
//         osi_SyncObjWait(&Rs485ws_Binary, OSI_WAIT_FOREVER); //waite UART1 interrupt message

//         rs485_init(UART1_SLOW_BAUD_RATE); //rs485 init

//         Rs485_WindSpeed_Sensor_val(&WindSpeed_val);

//         rs485_uinit(); //rs485 uinit

//         if (WindSpeed_val != ERROR_CODE)
//         {
//             rs485thMsg.sensornum = r1_ws_f_num; //Message Number

//             rs485thMsg.sensorval = WindSpeed_val; //Message Value

//             osi_MsgQWrite(&Data_Queue, &rs485thMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_WS_Status(1); //sensor exist status

//             if (lcd_display_num == WS_SENSOR_NUM)
//             {
//                 clear_sensor_Status(); //clear all sensors data status

//                 Display_WS_dataStatus(1);

//                 HT9B95A_clear_up_area(); //clear updisplay area

//                 HT9B95A_Display_Wind_Speed(WindSpeed_val); //Display the wind speed value
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }
//         else
//         {
//             osi_TaskDisable(); //disable the tasks

//             Display_WS_Status(0); //sensor inexistence status

//             if (lcd_display_num == WS_SENSOR_NUM)
//             {
//                 HT9B95A_clear_up_area();

//                 HT9B95A_clear_down_area();
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }
//     }
// }

// /*******************************************************************************
// //UART1 To RS485 Light sensor value
// *******************************************************************************/
// void Rs485_Light_Sensor_val(float *light_val)
// {
//     uint16_t th_crc16_val;
//     unsigned char rs485th_cmd[] = {0x0A, 0x04, 0x00, 0x00, 0x00, 0x02, 0x70, 0xB0};

//     Uart1_Rev_Init(); //clear the recive buffer

//     osi_Uart1_Write(rs485th_cmd, sizeof(rs485th_cmd)); //Send the command

//     MAP_UtilsDelay(2000000); //delay about 150ms

// #ifdef DEBUG
//     UART_PRINT("lt:%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\r\n", Uart1_Buf[0], Uart1_Buf[1], Uart1_Buf[2], Uart1_Buf[3], Uart1_Buf[4], Uart1_Buf[5], Uart1_Buf[6], Uart1_Buf[7], Uart1_Buf[8]);
// #endif
//     if (Uart_recv_buf_check((char *)rs485th_cmd, Uart1_Buf, sizeof(rs485th_cmd)))
//         th_crc16_val = CRC16_ModBus((uint8_t *)Uart1_Buf, 7); //Check the data
//     if (((th_crc16_val / 256) == Uart1_Buf[7]) && ((th_crc16_val % 256) == Uart1_Buf[8]))
//     {
//         *light_val = 16777216 * Uart1_Buf[3] + 65536 * Uart1_Buf[4] + 256 * Uart1_Buf[5] + Uart1_Buf[6];
//     }
//     else
//     {
//         *light_val = ERROR_CODE;
//     }
// }

// /*******************************************************************************
// //UART1 To RS485 Light sensor task
// *******************************************************************************/
// void Rs485_Light_Sensor(void *pvParameters)
// {
//     float Light_val;
//     SensorMessage rs485thMsg;

//     for (;;)
//     {
//         osi_SyncObjWait(&Rs485lt_Binary, OSI_WAIT_FOREVER); //waite UART1 interrupt message

//         rs485_init(UART1_SLOW_BAUD_RATE); //rs485 init

//         Rs485_Light_Sensor_val(&Light_val);

//         rs485_uinit(); //rs485 uinit

//         if (Light_val != ERROR_CODE)
//         {
//             rs485thMsg.sensornum = r1_light_f_num; //Message Number

//             rs485thMsg.sensorval = Light_val; //Message Value

//             osi_MsgQWrite(&Data_Queue, &rs485thMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_LIGHT_Status(1); //sensor exist status

//             if (lcd_display_num == LIGHT_SENSOR_NUM)
//             {
//                 clear_sensor_Status(); //clear all sensors data status

//                 Display_LIGHT_dataStatus(1);

//                 HT9B95A_Display_Light_Val(Light_val); //Display the light value

//                 HT9B95A_clear_down_area(); //clear down display area
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }
//         else
//         {
//             osi_TaskDisable(); //disable the tasks

//             Display_LIGHT_Status(0); //sensor inexistence status

//             if (lcd_display_num == LIGHT_SENSOR_NUM)
//             {
//                 HT9B95A_clear_up_area();

//                 HT9B95A_clear_down_area();
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }
//     }
// }

// /*******************************************************************************
// //CO2 and temp/huimi value change
// *******************************************************************************/
// float unsignedchar_to_float(char *c_buf)
// {
//     float Concentration_val;
//     unsigned long tempU32;

//     //cast 4 bytes to one unsigned 32 bit integer
//     tempU32 = (unsigned long)((((unsigned long)c_buf[0]) << 24) |
//                               (((unsigned long)c_buf[1]) << 16) |
//                               (((unsigned long)c_buf[2]) << 8) |
//                               ((unsigned long)c_buf[3]));

//     //cast unsigned 32 bit integer to 32 bit float
//     Concentration_val = *(float *)&tempU32; //

//     return Concentration_val;
// }

// /*******************************************************************************
// //UART1 To RS485 CO2 and temp/huimi sensor value
// *******************************************************************************/
// void Rs485_CO2_Sensor_val(float *co2_val, float *t_val, float *h_val)
// {
//     uint8_t i;
//     uint16_t th_crc16_val;
//     char val_buf[4] = {0};
//     unsigned char co2_tcmd[] = {0x61, 0x06, 0x00, 0x36, 0x00, 0x00, 0x60, 0x64};
//     unsigned char co2_scmd[] = {0x61, 0x06, 0x00, 0x25, 0x00, 0x02, 0x10, 0x60};
//     unsigned char co2_gcmd[] = {0x61, 0x03, 0x00, 0x27, 0x00, 0x01, 0x3D, 0xA1};
//     unsigned char co2_rcmd[] = {0x61, 0x03, 0x00, 0x28, 0x00, 0x06, 0x4C, 0x60};

//     *co2_val = ERROR_CODE;

//     *t_val = ERROR_CODE;

//     *h_val = ERROR_CODE;

//     Uart1_Rev_Init(); //clear the recive buffer

//     osi_Uart1_Write(co2_tcmd, sizeof(co2_tcmd)); //Send the command

//     MAP_UtilsDelay(2000000); //delay about 150ms

// #ifdef DEBUG
//     UART_PRINT("co2:%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\r\n", Uart1_Buf[0], Uart1_Buf[1], Uart1_Buf[2], Uart1_Buf[3], Uart1_Buf[4], Uart1_Buf[5], Uart1_Buf[6], Uart1_Buf[7]);
// #endif

//     th_crc16_val = CRC16_ModBus((uint8_t *)Uart1_Buf, 6); //Check the data
//     if (((th_crc16_val / 256) == Uart1_Buf[6]) && ((th_crc16_val % 256) == Uart1_Buf[7]))
//     {
//         Uart1_Rev_Init(); //clear the recive buffer

//         osi_Uart1_Write(co2_scmd, sizeof(co2_scmd)); //Send the command

//         MAP_UtilsDelay(2000000); //delay about 150ms

// #ifdef DEBUG
//         UART_PRINT("co2s:%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\r\n", Uart1_Buf[0], Uart1_Buf[1], Uart1_Buf[2], Uart1_Buf[3], Uart1_Buf[4], Uart1_Buf[5], Uart1_Buf[6], Uart1_Buf[7]);
// #endif
//         th_crc16_val = CRC16_ModBus((uint8_t *)Uart1_Buf, 6); //Check the data
//         if (((th_crc16_val / 256) == Uart1_Buf[6]) && ((th_crc16_val % 256) == Uart1_Buf[7]))
//         {
//             for (i = 0; i < 10; i++)
//             {
//                 MAP_UtilsDelay(20000000); //delay about 1.5ms

//                 Uart1_Rev_Init(); //clear the recive buffer

//                 osi_Uart1_Write(co2_gcmd, sizeof(co2_gcmd)); //Send the command

//                 MAP_UtilsDelay(2000000); //delay about 150ms

// #ifdef DEBUG
//                 UART_PRINT("co2g:%02x,%02x,%02x,%02x,%02x,%02x,%02x\r\n", Uart1_Buf[0], Uart1_Buf[1], Uart1_Buf[2], Uart1_Buf[3], Uart1_Buf[4], Uart1_Buf[5], Uart1_Buf[6]);
// #endif
//                 th_crc16_val = CRC16_ModBus((uint8_t *)Uart1_Buf, 5); //Check the data
//                 if (((th_crc16_val / 256) == Uart1_Buf[5]) && ((th_crc16_val % 256) == Uart1_Buf[6]))
//                 {
//                     if (Uart1_Buf[4] == 0x01)
//                     {
//                         Uart1_Rev_Init(); //clear the recive buffer

//                         osi_Uart1_Write(co2_rcmd, sizeof(co2_rcmd)); //Send the command

//                         MAP_UtilsDelay(2000000); //delay about 150ms

// #ifdef DEBUG
//                         UART_PRINT("co2r:%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\r\n", Uart1_Buf[0], Uart1_Buf[1], Uart1_Buf[2], Uart1_Buf[3], Uart1_Buf[4], Uart1_Buf[5], Uart1_Buf[6], Uart1_Buf[7], Uart1_Buf[8], Uart1_Buf[9], Uart1_Buf[10], Uart1_Buf[11], Uart1_Buf[12], Uart1_Buf[13], Uart1_Buf[14], Uart1_Buf[15], Uart1_Buf[16]);
// #endif
//                         th_crc16_val = CRC16_ModBus((uint8_t *)Uart1_Buf, 15); //Check the data
//                         if (((th_crc16_val / 256) == Uart1_Buf[15]) && ((th_crc16_val % 256) == Uart1_Buf[16]))
//                         {
//                             mem_copy(val_buf, Uart1_Buf + 3, 4);
// #ifdef DEBUG
//                             UART_PRINT("co2:%02x,%02x,%02x,%02x\r\n", val_buf[0], val_buf[1], val_buf[2], val_buf[3]);
// #endif
//                             *co2_val = unsignedchar_to_float(val_buf);
// #ifdef DEBUG
//                             UART_PRINT("co2:%f\r\n", *co2_val);
// #endif
//                             mem_copy(val_buf, Uart1_Buf + 7, 4);
// #ifdef DEBUG
//                             UART_PRINT("t:%02x,%02x,%02x,%02x\r\n", val_buf[0], val_buf[1], val_buf[2], val_buf[3]);
// #endif
//                             *t_val = unsignedchar_to_float(val_buf);
// #ifdef DEBUG
//                             UART_PRINT("t:%f\r\n", *t_val);
// #endif
//                             mem_copy(val_buf, Uart1_Buf + 11, 4);
// #ifdef DEBUG
//                             UART_PRINT("h:%02x,%02x,%02x,%02x\r\n", val_buf[0], val_buf[1], val_buf[2], val_buf[3]);
// #endif
//                             *h_val = unsignedchar_to_float(val_buf);
// #ifdef DEBUG
//                             UART_PRINT("h:%f\r\n", *h_val);
// #endif
//                             if ((*co2_val >= 400) && (*co2_val <= 10000))
//                                 break;
//                         }
//                     }
//                 }
//             }
//             if ((*co2_val < 400) && (*co2_val > 10000))
//                 *co2_val = ERROR_CODE;
//         }
//     }
// }

// /*******************************************************************************
// //UART1 To RS485 CO2 sensor task
// *******************************************************************************/
// void Rs485_CO2_Sensor(void *pvParameters)
// {
//     float s_val1, s_val2, s_val3;
//     SensorMessage rs485co2Msg;

//     for (;;)
//     {
//         osi_SyncObjWait(&Rs485co2_Binary, OSI_WAIT_FOREVER); //waite UART1 interrupt message

//         rs485_init(UART1_MIDDLE_BAUD_RATE); //rs485 init

//         Rs485_CO2_Sensor_val(&s_val1, &s_val2, &s_val3);

//         rs485_uinit(); //rs485 uinit

//         if ((s_val1 != ERROR_CODE) && (s_val2 != ERROR_CODE) && (s_val3 != ERROR_CODE))
//         {
//             rs485co2Msg.sensornum = r1_co2_f_num; //Message Number

//             rs485co2Msg.sensorval = s_val1; //Message Value

//             osi_MsgQWrite(&Data_Queue, &rs485co2Msg, OSI_NO_WAIT); //Send Temperature Data Message

//             rs485co2Msg.sensornum = r1_th_t_f_num; //Message Number

//             rs485co2Msg.sensorval = s_val2; //Message Value

//             osi_MsgQWrite(&Data_Queue, &rs485co2Msg, OSI_NO_WAIT); //Send Temperature Data Message

//             rs485co2Msg.sensornum = r1_th_h_f_num; //Message Number

//             rs485co2Msg.sensorval = s_val3; //Message Value

//             osi_MsgQWrite(&Data_Queue, &rs485co2Msg, OSI_NO_WAIT); //Send Humility Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_CO2_Status(1); //sensor exist status

//             if (lcd_display_num == CO2_SENSOR_NUM)
//             {
//                 clear_sensor_Status(); //clear all sensors data status

//                 Display_CO2_dataStatus(1);

//                 HT9B95A_clear_up_area(); //clear updisplay area

//                 HT9B95A_Display_CO2_val(s_val1);
//             }
//             else if (lcd_display_num == CO2TH_SENSOR_NUM)
//             {
//                 HT9B95A_Display_Temp_Val(s_val2, C_F_Temp); //Display the temprature value

//                 HT9B95A_Display_Humi_Val((uint8_t)s_val3); //Display the Humility value
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }
//         else
//         {
//             osi_TaskDisable(); //disable the tasks

//             Display_CO2_Status(0); //sensor inexistence status

//             if (lcd_display_num == CO2_SENSOR_NUM)
//             {
//                 HT9B95A_clear_up_area();

//                 HT9B95A_clear_down_area();
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }
//     }
// }

// /*******************************************************************************
// //Sensors check task
// *******************************************************************************/
// void Sensors_status_check(void *pvParameters)
// {
//     SensorMessage sMsg;
//     float s_val1, s_val2, s_val3;
//     float th_val1, th_val2;

//     for (;;)
//     {
//         osi_SyncObjWait(&Sensors_check_Binary, OSI_WAIT_FOREVER); //Wait task operation Message

//         sensor_check_processing = 1;

//         osi_SyncObjSignalFromISR(&check_process_Binary); //Start Tasks Check

//         if (!SYS_RESTART)
//         {
//             lcd_display_num += 1;
//         }
//         else
//         {
//             SYS_RESTART = 0;
//         }
//         lcd_display_num = lcd_display_num > MAX_SENSOR_NUM ? DEFAULT_LCD_DIS_NUM : lcd_display_num;

//         osi_Read_UnixTime(); //update system unix time

//         //temp&huimi sensor
//         osi_sht30_SingleShotMeasure(&th_val1, &th_val2); //read temperature humility data
//         if ((th_val1 != ERROR_CODE) && (th_val2 != ERROR_CODE))
//         {
//             sMsg.sensornum = th_t_f_num; //Message Number

//             sMsg.sensorval = th_val1; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             sMsg.sensornum = th_h_f_num; //Message Number

//             sMsg.sensorval = th_val2; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Humility Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_TH_Status(1); //sensor exist status

//             if (lcd_display_num == TH_SENSOR_NUM)
//             {
//                 HT9B95A_Display_Temp_Val(th_val1, C_F_Temp); //Display the temprature value

//                 HT9B95A_Display_Humi_Val((uint8_t)th_val2); //Display the Humility value
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }
//         else
//         {
//             osi_TaskDisable(); //disable the tasks

//             Display_TH_Status(0); //sensor inexistence status

//             osi_TaskEnable(0); //enable all tasks

//             if (lcd_display_num == TH_SENSOR_NUM)
//             {
//                 lcd_display_num += 1;
//             }
//         }

//         rs485_init(UART1_SLOW_BAUD_RATE); //rs485 init

//         //rs485 temp&huimi sensor
//         Rs485_th_Sensor_val(&s_val1, &s_val2); //RS485 TH sensor
//         if ((s_val1 != ERROR_CODE) && (s_val2 != ERROR_CODE))
//         {
//             sMsg.sensornum = r1_th_t_f_num; //Message Number

//             sMsg.sensorval = s_val1; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             sMsg.sensornum = r1_th_h_f_num; //Message Number

//             sMsg.sensorval = s_val2; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Humility Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_E_TH_Status(1); //sensor exist status

//             if (lcd_display_num == E_TH_SENSOR_NUM)
//             {
//                 HT9B95A_Display_Temp_Val(s_val1, C_F_Temp); //Display the temprature value

//                 HT9B95A_Display_Humi_Val((uint8_t)s_val2); //Display the Humility value
//             }

//             osi_TaskEnable(0); //enable all tasks

//             if (fn_485_th == 0)
//             {
//                 fn_485_th = DEFAULT_RS485_FN;

//                 osi_at24c08_write(FN_485_TH_ADDR, fn_485_th); //fn_485_th

//                 fn_485_th_t = now_unix_t + fn_485_th;

//                 osi_at24c08_write(FN_485_TH_T_ADDR, fn_485_th_t);
//             }
//         }
//         else
//         {
//             osi_TaskDisable(); //disable the tasks

//             Display_E_TH_Status(0); //sensor inexistence status

//             osi_TaskEnable(0); //enable all tasks

//             if (lcd_display_num == E_TH_SENSOR_NUM)
//             {
//                 lcd_display_num += 1;
//             }
//         }

//         osi_Sleep(500); //delay about 0.5s

//         //rs485 soil temp&huimi sensor
//         Rs485_s_th_Sensor_val(&s_val1, &s_val2);
//         if ((s_val1 != ERROR_CODE) && (s_val2 != ERROR_CODE))
//         {
//             sMsg.sensornum = r1_sth_t_f_num; //Message Number

//             sMsg.sensorval = s_val1; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             sMsg.sensornum = r1_sth_h_f_num; //Message Number

//             sMsg.sensorval = s_val2; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Humility Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_S_TH_Status(1); //sensor exist status

//             if (lcd_display_num == S_TH_SENSOR_NUM)
//             {
//                 HT9B95A_Display_Temp_Val(s_val1, C_F_Temp); //Display the temprature value

//                 HT9B95A_Display_Humi_Val((uint8_t)s_val2); //Display the Humility value
//             }

//             osi_TaskEnable(0); //enable all tasks

//             if (fn_485_sth == 0)
//             {
//                 fn_485_sth = DEFAULT_RS485_FN;

//                 osi_at24c08_write(FN_485_STH_ADDR, fn_485_sth); //fn_485_sth

//                 fn_485_sth_t = now_unix_t + fn_485_sth;

//                 osi_at24c08_write(FN_485_STH_T_ADDR, fn_485_sth_t);
//             }
//         }
//         else
//         {
//             osi_TaskDisable(); //disable the tasks

//             Display_S_TH_Status(0); //sensor inexistence status

//             osi_TaskEnable(0); //enable all tasks

//             if (lcd_display_num == S_TH_SENSOR_NUM)
//             {
//                 lcd_display_num += 1;
//             }
//         }

//         osi_Sleep(500); //delay about 0.5s

//         Rs485_t_Sensor_val(&s_val1); //rs485 temp sensor
//         if (s_val1 != ERROR_CODE)
//         {
//             sMsg.sensornum = r1_t_f_num; //Message Number

//             sMsg.sensorval = s_val1; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_E_T_Status(1); //sensor exist status

//             if (lcd_display_num == E_T_SENSOR_NUM)
//             {
//                 HT9B95A_Display_Temp_Val(s_val1, C_F_Temp); //Display the temprature value

//                 HT9B95A_clear_down_area(); //clear down display area
//             }

//             osi_TaskEnable(0); //enable all tasks

//             if (fn_485_t == 0)
//             {
//                 fn_485_t = DEFAULT_RS485_FN;

//                 osi_at24c08_write(FN_485_T_ADDR, fn_485_t); //fn_485_t

//                 fn_485_t_t = now_unix_t + fn_485_t;

//                 osi_at24c08_write(FN_485_T_T_ADDR, fn_485_t_t);
//             }
//         }

//         osi_ds18b20_get_temp(&s_val2); ////DS18B20 ext temp sensor measure the temperature
//         if (s_val2 != ERROR_CODE)
//         {
//             sMsg.sensornum = e1_t_f_num; //Message Number

//             sMsg.sensorval = s_val2; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_E_T_Status(1); //sensor exist status

//             if (lcd_display_num == E_T_SENSOR_NUM)
//             {
//                 HT9B95A_Display_Temp_Val(s_val2, C_F_Temp); //Display the temprature value

//                 HT9B95A_clear_down_area(); //clear down display area
//             }

//             osi_TaskEnable(0); //enable all tasks

//             if (fn_ext == 0)
//             {
//                 fn_ext = DEFAULT_RS485_FN;

//                 osi_at24c08_write(FN_EXT_ADDR, fn_ext); //fn_ext

//                 fn_ext_t = now_unix_t + fn_ext;

//                 osi_at24c08_write(FN_EXT_T_ADDR, fn_ext_t);
//             }
//         }

//         if ((s_val1 == ERROR_CODE) && (s_val2 == ERROR_CODE))
//         {
//             osi_TaskDisable(); //disable the tasks

//             Display_E_T_Status(0); //sensor inexistence status

//             osi_TaskEnable(0); //enable all tasks

//             if (lcd_display_num == E_T_SENSOR_NUM)
//             {
//                 lcd_display_num += 1;
//             }
//         }

//         //light sensor or rs485 light sensor
//         osi_OPT3001_value(&s_val1); //Read Light Value
//         if (s_val1 != ERROR_CODE)
//         {
//             sMsg.sensornum = light_f_num; //Message Number

//             sMsg.sensorval = s_val1; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_LIGHT_Status(1); //sensor exist status

//             if (lcd_display_num == LIGHT_SENSOR_NUM)
//             {
//                 HT9B95A_Display_Light_Val(s_val1); //Display the light value

//                 HT9B95A_clear_down_area(); //clear down display area
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }

//         osi_Sleep(500); //delay about 0.5s

//         Rs485_Light_Sensor_val(&s_val2);
//         if (s_val2 != ERROR_CODE)
//         {
//             if ((fn_485_lt == 0))
//             {
//                 fn_485_lt = DEFAULT_RS485_FN;

//                 osi_at24c08_write(FN_485_LT_ADDR, fn_485_lt); //fn_485_lt

//                 fn_485_lt_t = now_unix_t + fn_485_lt;

//                 osi_at24c08_write(FN_485_LT_T_ADDR, fn_485_lt_t);
//             }

//             sMsg.sensornum = r1_light_f_num; //Message Number

//             sMsg.sensorval = s_val2; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_LIGHT_Status(1); //sensor exist status

//             if (lcd_display_num == LIGHT_SENSOR_NUM)
//             {
//                 HT9B95A_Display_Light_Val(s_val2); //Display the light value

//                 HT9B95A_clear_down_area(); //clear down display area
//             }

//             osi_TaskEnable(0); //enable all tasks
//         }

//         if ((s_val1 == ERROR_CODE) && (s_val2 == ERROR_CODE))
//         {
//             osi_TaskDisable(); //disable the tasks

//             Display_LIGHT_Status(0); //sensor inexistence status

//             osi_TaskEnable(0); //enable all tasks

//             if (lcd_display_num == LIGHT_SENSOR_NUM)
//             {
//                 lcd_display_num += 1;
//             }
//         }

//         osi_Sleep(500); //delay about 0.5s

//         //rs485 wind speed sensor
//         Rs485_WindSpeed_Sensor_val(&s_val1);
//         if (s_val1 != ERROR_CODE)
//         {
//             sMsg.sensornum = r1_ws_f_num; //Message Number

//             sMsg.sensorval = s_val1; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_WS_Status(1); //sensor exist status

//             if (lcd_display_num == WS_SENSOR_NUM)
//             {
//                 HT9B95A_clear_up_area(); //clear updisplay area

//                 HT9B95A_Display_Wind_Speed(s_val1); //Display the wind speed value
//             }

//             osi_TaskEnable(0); //enable all tasks

//             if (fn_485_ws == 0)
//             {
//                 fn_485_ws = DEFAULT_RS485_FN;

//                 osi_at24c08_write(FN_485_WS_ADDR, fn_485_ws); //fn_485_ws

//                 fn_485_ws_t = now_unix_t + fn_485_ws;

//                 osi_at24c08_write(FN_485_WS_T_ADDR, fn_485_ws_t);
//             }
//         }
//         else
//         {
//             osi_TaskDisable(); //disable the tasks

//             Display_WS_Status(0); //sensor inexistence status

//             osi_TaskEnable(0); //enable all tasks

//             if (lcd_display_num == WS_SENSOR_NUM)
//             {
//                 lcd_display_num += 1;
//             }
//         }

//         osi_Sleep(500); //delay about 0.5s

//         Uart1_Disable(); //Disable UartA1 Peripheral

//         Set_Uart1_Int_Source(UART1_MIDDLE_BAUD_RATE); //Set Uart1 Interrupt Source

//         Rs485_CO2_Sensor_val(&s_val1, &s_val2, &s_val3);
//         if (s_val1 != ERROR_CODE)
//         {
//             sMsg.sensornum = r1_co2_f_num; //Message Number

//             sMsg.sensorval = s_val1; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             sMsg.sensornum = r1_th_t_f_num; //Message Number

//             sMsg.sensorval = s_val2; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Temperature Data Message

//             sMsg.sensornum = r1_th_h_f_num; //Message Number

//             sMsg.sensorval = s_val3; //Message Value

//             osi_MsgQWrite(&Data_Queue, &sMsg, OSI_NO_WAIT); //Send Humility Data Message

//             osi_TaskDisable(); //disable the tasks

//             Display_CO2_Status(1); //sensor exist status

//             if (lcd_display_num == CO2_SENSOR_NUM)
//             {
//                 HT9B95A_clear_up_area(); //clear updisplay area

//                 HT9B95A_Display_CO2_val(s_val1);
//             }
//             else if (lcd_display_num == CO2TH_SENSOR_NUM)
//             {
//                 HT9B95A_Display_Temp_Val(s_val2, C_F_Temp); //Display the temprature value

//                 HT9B95A_Display_Humi_Val((uint8_t)s_val3); //Display the Humility value
//             }

//             osi_TaskEnable(0); //enable all tasks

//             if (fn_485_co2 == 0)
//             {
//                 fn_485_co2 = DEFAULT_RS485_FN;

//                 osi_at24c08_write(FN_485_CO2_ADDR, fn_485_co2); //fn_485_co2

//                 fn_485_co2_t = now_unix_t + fn_485_co2;

//                 osi_at24c08_write(FN_485_CO2_T_ADDR, fn_485_co2_t);
//             }
//         }
//         else
//         {
//             osi_TaskDisable(); //disable the tasks

//             Display_CO2_Status(0); //sensor inexistence status

//             osi_TaskEnable(0); //enable all tasks

//             if (lcd_display_num == CO2_SENSOR_NUM)
//             {
//                 lcd_display_num += 2;
//             }
//         }

//         rs485_uinit(); //rs485 uinit

//         if (lcd_display_num > MAX_SENSOR_NUM)
//         {
//             if ((th_val1 != ERROR_CODE) && (th_val2 != ERROR_CODE))
//             {
//                 sensor_check_processing = 0;

//                 lcd_display_num = TH_SENSOR_NUM;

//                 osi_TaskDisable(); //disable the tasks

//                 HT9B95A_Display_Temp_Val(th_val1, C_F_Temp); //Display the temprature value

//                 HT9B95A_Display_Humi_Val((uint8_t)th_val2); //Display the Humility value

//                 osi_TaskEnable(0); //enable all tasks
//             }
//             else
//             {
//                 osi_SyncObjSignalFromISR(&Sensors_check_Binary);
//             }
//         }
//         else
//         {
//             sensor_check_processing = 0;

//             osi_SyncObjClear(&Sensors_check_Binary);
//         }

//         osi_at24c08_write_byte(LCD_DIS_NUM_ADDR, lcd_display_num); //lcd_display_num
//     }
// }

// /*******************************************************************************
// //Sensors check process
// *******************************************************************************/
// void Sensors_check_process(void *pvParameters)
// {
//     for (;;)
//     {
//         osi_SyncObjWait(&check_process_Binary, OSI_WAIT_FOREVER); //Wait task operation Message

//         SENSOR_CHECK_END = 1;

//         osi_SyncObjSignalFromISR(&Wait_Binary); //Start Tasks End Check

//         osi_TaskDisable();     //disable the tasks
//         clear_sensor_Status(); //clear all sensors data status
//         osi_TaskEnable(0);     //enable all tasks

//         osi_TaskDisable(); //disable the tasks
//         Display_TH_dataStatus(1);
//         osi_TaskEnable(0); //enable all tasks

//         osi_Sleep(500); //delay about 0.5s

//         osi_TaskDisable(); //disable the tasks
//         Display_E_TH_dataStatus(1);
//         osi_TaskEnable(0); //enable all tasks

//         osi_Sleep(500); //delay about 0.5s

//         osi_TaskDisable(); //disable the tasks
//         Display_S_TH_dataStatus(1);
//         osi_TaskEnable(0); //enable all tasks

//         osi_Sleep(500); //delay about 0.5s

//         osi_TaskDisable(); //disable the tasks
//         Display_E_T_dataStatus(1);
//         osi_TaskEnable(0); //enable all tasks

//         osi_Sleep(500); //delay about 0.5s

//         osi_TaskDisable(); //disable the tasks
//         Display_LIGHT_dataStatus(1);
//         osi_TaskEnable(0); //enable all tasks

//         osi_Sleep(500); //delay about 0.5s

//         osi_TaskDisable(); //disable the tasks
//         Display_WS_dataStatus(1);
//         osi_TaskEnable(0); //enable all tasks

//         osi_Sleep(500); //delay about 0.5s

//         osi_TaskDisable(); //disable the tasks
//         Display_CO2_dataStatus(1);
//         osi_TaskEnable(0); //enable all tasks

//         osi_Sleep(500); //delay about 0.5s

//         osi_TaskDisable(); //disable the tasks
//         Display_PH_dataStatus(1);
//         osi_TaskEnable(0); //enable all tasks

//         osi_Sleep(500); //delay about 0.5s

//         while (sensor_check_processing)
//         {
//             osi_TaskDisable();     //disable the tasks
//             clear_sensor_Status(); //clear all sensors data status
//             osi_TaskEnable(0);     //enable all tasks

//             osi_Sleep(500); //delay about 0.5s

//             osi_TaskDisable(); //disable the tasks
//             Display_TH_dataStatus(1);
//             Display_E_TH_dataStatus(1);
//             Display_S_TH_dataStatus(1);
//             Display_E_T_dataStatus(1);
//             Display_LIGHT_dataStatus(1);
//             Display_WS_dataStatus(1);
//             Display_CO2_dataStatus(1);
//             Display_PH_dataStatus(1);
//             osi_TaskEnable(0); //enable all tasks

//             osi_Sleep(500); //delay about 0.5s
//         }

//         osi_TaskDisable();     //disable the tasks
//         clear_sensor_Status(); //clear all sensors data status
//         if (lcd_display_num == TH_SENSOR_NUM)
//         {
//             Display_TH_dataStatus(1);
//         }
//         else if (lcd_display_num == E_TH_SENSOR_NUM)
//         {
//             Display_E_TH_dataStatus(1);
//         }
//         else if (lcd_display_num == S_TH_SENSOR_NUM)
//         {
//             Display_S_TH_dataStatus(1);
//         }
//         else if (lcd_display_num == E_T_SENSOR_NUM)
//         {
//             Display_E_T_dataStatus(1);
//         }
//         else if (lcd_display_num == LIGHT_SENSOR_NUM)
//         {
//             Display_LIGHT_dataStatus(1);
//         }
//         else if (lcd_display_num == WS_SENSOR_NUM)
//         {
//             Display_WS_dataStatus(1);
//         }
//         else if ((lcd_display_num == CO2_SENSOR_NUM) || (lcd_display_num == CO2TH_SENSOR_NUM))
//         {
//             Display_CO2_dataStatus(1);
//         }
//         else if (lcd_display_num == PH_SENSOR_NUM)
//         {
//             Display_PH_dataStatus(1);
//         }
//         osi_TaskEnable(0); //enable all tasks

//         osi_SyncObjClear(&check_process_Binary);

//         SENSOR_CHECK_END = 0;
//     }
// }

// /*******************************************************************************
//                                       END
// *******************************************************************************/