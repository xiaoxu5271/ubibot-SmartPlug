#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "driver/uart.h"
#include "soc/uart_periph.h"
#include "driver/gpio.h"
#include "Json_parse.h"
#include "Smartconfig.h"
#include "Led.h"

#include "EC20.h"

#define UART1_TXD (GPIO_NUM_21)
#define UART1_RXD (GPIO_NUM_22)

static const char *TAG = "EC20";
TaskHandle_t EC20_Task_Handle;
TaskHandle_t Uart1_Task_Handle;

#define EC20_SW 25
#define BUF_SIZE 1024

#define EX_UART_NUM UART_NUM_1

bool EC20_NET_STA = false;

static QueueHandle_t EC_uart_queue;
// static QueueHandle_t EC_at_queue;
static SemaphoreHandle_t EC20_at_Binary;

uint8_t EC20_RECV[BUF_SIZE];
char ICCID[24] = {0};

void Uart1_Task(void *arg);
void EC20_Task(void *arg);

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint16_t all_read_len = 0;

    for (;;)
    {
        //Waiting for UART event.
        if (xQueueReceive(EC_uart_queue, (void *)&event, (portTickType)portMAX_DELAY))
        {
            switch (event.type)
            {
            case UART_DATA:
                // ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                if (event.size > 1)
                {
                    if (all_read_len + event.size >= BUF_SIZE)
                    {
                        ESP_LOGE(TAG, "read len flow");
                        all_read_len = 0;
                        memset(EC20_RECV, 0, BUF_SIZE);
                    }
                    uart_read_bytes(EX_UART_NUM, EC20_RECV + all_read_len, event.size, portMAX_DELAY);
                    all_read_len += event.size;

                    EC20_RECV[all_read_len] = 0; //去掉字符串结束符，防止字符串拼接不成功
                    if (EC20_RECV[all_read_len - 1] == '\n')
                    {
                        if (strstr((char *)EC20_RECV, "+QMTRECV:") != NULL)
                        {
                            ESP_LOGI("MQTT", "%s\n", EC20_RECV);
                        }
                        else if (strstr((char *)EC20_RECV, "+QMTSTAT:") != NULL)
                        {
                            ESP_LOGE("MQTT", "%s\n", EC20_RECV);
                            if (eTaskGetState(EC20_Task_Handle) == eSuspended)
                            {
                                vTaskResume(EC20_Task_Handle);
                            }
                        }
                        else if (strstr((char *)EC20_RECV, "+QMTPING:") != NULL)
                        {
                            ESP_LOGE("MQTT", "%s\n", EC20_RECV);
                            if (eTaskGetState(EC20_Task_Handle) == eSuspended)
                            {
                                vTaskResume(EC20_Task_Handle);
                            }
                        }
                        else if (strstr((char *)EC20_RECV, "+CME ERROR:") != NULL)
                        {
                            ESP_LOGE("MQTT", "%s\n", EC20_RECV);
                            if (eTaskGetState(EC20_Task_Handle) == eSuspended)
                            {
                                vTaskResume(EC20_Task_Handle);
                            }
                        }
                        else
                        {
                            // xQueueSend(EC_at_queue, (void *)EC20_RECV, 0);
                            xSemaphoreGive(EC20_at_Binary);
                        }
                        all_read_len = 0;
                        memset(EC20_RECV, 0, BUF_SIZE);
                        uart_flush(EX_UART_NUM);
                    }
                }
                break;

            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                uart_flush_input(EX_UART_NUM);
                xQueueReset(EC_uart_queue);
                break;

            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                uart_flush_input(EX_UART_NUM);
                xQueueReset(EC_uart_queue);
                break;

            //Others
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    vTaskDelete(NULL);
}

void EC20_Start(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, 0, 2, &EC_uart_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);
    uart_set_pin(EX_UART_NUM, UART1_TXD, UART1_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // //uart2 switch io
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1 << EC20_SW);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    EC20_at_Binary = xSemaphoreCreateBinary();
    // EC_at_queue = xQueueCreate(2, BUF_SIZE);

    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
    xTaskCreate(EC20_Task, "EC20_Task", 2048, NULL, 9, &EC20_Task_Handle);
}

void EC20_Power_On(void)
{
    //开机
    gpio_set_level(EC20_SW, 1); //
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(EC20_SW, 0); //
    // vTaskDelay(15000 / portTICK_PERIOD_MS);
    AT_Cmd_Send(NULL, "RDY", 10000, 1);
}

void EC20_Rest(void)
{
    if (AT_Cmd_Send("AT+QPOWD=0\r\n", "POWERED DOWN", 1000, 1) != NULL)
    {
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
    EC20_Power_On();
}

void EC20_Task(void *arg)
{
    uint8_t ret;
    while (1)
    {
        EC20_NET_STA = false;
        if (WIFI_STA == false)
        {
            Led_Status = LED_STA_WIFIERR;
            xEventGroupClearBits(Net_sta_group, CONNECTED_BIT);
        }
        if (net_mode != NET_WIFI)
        {
            ret = EC20_Init();
            if (ret == 0)
            {
                continue;
            }
            ret = EC20_Http_CFG();
            if (ret == 0)
            {
                continue;
            }
            // ret = EC20_MQTT();
            // if (ret == 0)
            // {
            //     continue;
            // }

            EC20_NET_STA = true;
            xEventGroupSetBits(Net_sta_group, CONNECTED_BIT);
        }

        vTaskSuspend(NULL);
    }
}

/**********************************************************/

/*******************************************************************************
//Check AT Command Respon result，
// 
*******************************************************************************/
char *AT_Cmd_Send(char *cmd_buf, char *check_buff, uint32_t time_out, uint8_t try_num)
{
    char *rst_val = NULL;
    uint8_t i, j;
    // char *recv_buf = (char *)malloc(BUF_SIZE);

    for (i = 0; i < try_num; i++)
    {
        // xQueueReset(EC_at_queue);
        uart_flush(EX_UART_NUM);
        if (cmd_buf != NULL)
        {
            uart_write_bytes(EX_UART_NUM, cmd_buf, strlen(cmd_buf));
        }

        for (j = 0; j < 10; j++)
        {
            // if (xQueueReceive(EC_at_queue, (void *)recv_buf, time_out / portTICK_RATE_MS) == pdPASS)
            if (xSemaphoreTake(EC20_at_Binary, time_out / portTICK_RATE_MS) == pdTRUE)
            {
                rst_val = strstr((char *)EC20_RECV, check_buff); //
                if (rst_val != NULL)
                {
                    break;
                }
            }
            else //未等到数据
            {
                ESP_LOGI(TAG, "LINE %d", __LINE__);
                break;
            }
        }

        if (rst_val != NULL)
        {
            break;
        }
    }
    // free(recv_buf);
    return rst_val; //
}

uint8_t EC20_Net_Check(void)
{
    uint8_t ret = 0;
    for (uint8_t i = 0; i < 15; i++)
    {
        ESP_LOGI(TAG, "EC20 Net_ChecK");
        if (AT_Cmd_Send("AT+QIACT?\r\n", "+QIACT: 1,1", 1000, 5) != NULL)
        {
            ret = 1;
            break;
        }
        if (AT_Cmd_Send("AT+QIACT=1\r\n", "OK", 100, 5) == NULL)
        {
            ESP_LOGE(TAG, "EC20_Http_CFG %d", __LINE__);
        }
    }

    if (ret == 0) //重启
    {
        if (eTaskGetState(EC20_Task_Handle) == eSuspended)
        {
            vTaskResume(EC20_Task_Handle);
        }
    }

    return ret;
}

//EC20 init
uint8_t EC20_Init(void)
{
    char *ret;

    EC20_Rest();

    // ret = AT_Cmd_Send("ATE0\r\n", "OK", 100, 5); //回显
    // if (ret == NULL)
    // {
    //     ESP_LOGE(TAG, "ATE0  ");
    //     return 0;
    // }

    ret = AT_Cmd_Send("AT+IPR=115200\r\n", "OK", 100, 5);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Init %d", __LINE__);
        goto end;
    }

    ret = AT_Cmd_Send("AT+CPIN?\r\n", "READY", 100, 5);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Init %d", __LINE__);
        goto end;
    }

    ret = AT_Cmd_Send("AT+QCCID\r\n", "+QCCID:", 100, 5);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Init %d", __LINE__);
        goto end;
    }
    else
    {
        memcpy(ICCID, ret + 8, 20);
        ESP_LOGI(TAG, "ICCID=%s", ICCID);
    }

    ret = AT_Cmd_Send("AT+CGATT?\r\n", "+CGATT: 1", 1000, 10);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Init %d", __LINE__);
        goto end;
    }

end:
    // free(active_url);
    // free(cmd_buf);
    if (ret == NULL)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

uint8_t EC20_Http_CFG(void)
{
    char *ret;
    char *cmd_buf = (char *)malloc(120);

    ret = AT_Cmd_Send("AT+QHTTPCFG=\"contextid\",1\r\n", "OK", 100, 5);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Http_CFG %d", __LINE__);
        goto end;
    }

    ret = AT_Cmd_Send("AT+QHTTPCFG=\"responseheader\",0\r\n", "OK", 100, 5);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Http_CFG %d", __LINE__);
        goto end;
    }

    ret = AT_Cmd_Send("AT+QHTTPCFG=\"closewaittime\",0\r\n", "OK", 100, 5);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Http_CFG %d", __LINE__);
        goto end;
    }

    ret = AT_Cmd_Send("AT+QIACT?\r\n", "+QIACT: 1,1", 100, 5);
    if (ret != NULL)
    {
        // ESP_LOGI(TAG, "EC20_Http_CFG %d", __LINE__);
        goto end;
    }

    sprintf(cmd_buf, "AT+QICSGP=1,1,\"%s\",\"%s\",\"%s\",1\r\n", SIM_APN, SIM_USER, SIM_PWD);
    ret = AT_Cmd_Send(cmd_buf, "OK", 100, 5);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Http_CFG %d", __LINE__);
        goto end;
    }

    ret = AT_Cmd_Send("AT+QIACT=1\r\n", "OK", 100, 5);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Http_CFG %d", __LINE__);
        goto end;
    }

    ret = AT_Cmd_Send("AT+QIACT?\r\n", "+QIACT: 1,1", 100, 5);
    if (ret != NULL)
    {
        // ESP_LOGI(TAG, "EC20_Http_CFG %d", __LINE__);
        goto end;
    }

end:
    // free(active_url);
    free(cmd_buf);
    if (ret == NULL)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

uint8_t EC20_Active(char *active_url, char *recv_buf)
{

    // xSemaphoreTake(EC20_muxtex, -1);

    char *ret;
    char *cmd_buf;
    uint8_t active_len;
    cmd_buf = (char *)malloc(24);
    memset(cmd_buf, 0, 24);
    active_len = strlen(active_url) - 2; //不包含换行符
    sprintf(cmd_buf, "AT+QHTTPURL=%d,60\r\n", active_len);
    ret = AT_Cmd_Send(cmd_buf, "CONNECT", 1000, 1);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Active %d", __LINE__);
        goto end;
    }
    ret = AT_Cmd_Send(active_url, "OK", 60000, 1);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Active %d", __LINE__);
        goto end;
    }

    ret = AT_Cmd_Send("AT+QHTTPGET=60\r\n", "+QHTTPGET: 0,200", 6000, 1);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Active %d", __LINE__);
        goto end;
    }

    ret = AT_Cmd_Send("AT+QHTTPREAD=60\r\n", "{\"result\":", 1000, 1);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Active %d", __LINE__);
        goto end;
    }
    memcpy(recv_buf, ret, BUF_SIZE);

end:
    // xSemaphoreGive(EC20_muxtex);
    free(cmd_buf);
    if (ret == NULL)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

uint8_t EC20_Post_Data(void)
{
    char *ret;
    char *cmd_buf;
    char *post_buf;
    char *post_url;
    // uint16_t post_len;
    // uint16_t url_len;

    post_url = (char *)malloc(80);
    cmd_buf = (char *)malloc(30);
    post_buf = (char *)malloc(300);
    memset(post_url, 0, 80);
    memset(cmd_buf, 0, 30);
    memset(post_buf, 0, 300);

    sprintf(post_url, "http://api.ubibot.cn/update.json?api_key=30f29fe61a17c644315338535f91fa78\r\n");
    sprintf(post_buf, "{\"feeds\":[{\"created_at\":\"2020-02-27T08:49:32Z\",\"field1\":1,\"field2\":237,\"field3\":0,\"field4\":0,\"field6\":\"-54\"}],\"sensors\":[{\"rssi_g\":0,\"light\":0,\"r1_light\":0,\"r1_th_t\":0,\"r1_th_h\":0,\"r1_sth_t\":0,\"r1_sth_h\":0,\"e1_t\":0,\"r1_t\":0,\"r1_ws\":0,\"r1_co2\":0,\"r1_ph\":0}]}\r\n");

    const char *post_buf1 = "{\"feeds\":[";
    const char *post_buf2 = "{\"created_at\":\"2020-02-27T08:49:32Z\",\"field1\":1,\"field2\":237,\"field3\":0,\"field4\":0,\"field6\":\"-54\"}";
    const char *post_buf3 = "],\"sensors\":[{\"rssi_g\":0,\"light\":0,\"r1_light\":0,\"r1_th_t\":0,\"r1_th_h\":0,\"r1_sth_t\":0,\"r1_sth_h\":0,\"e1_t\":0,\"r1_t\":0,\"r1_ws\":0,\"r1_co2\":0,\"r1_ph\":0}]}";

    // url_len = strlen(post_url);

    sprintf(cmd_buf, "AT+QHTTPURL=%d,10\r\n", 73);
    ret = AT_Cmd_Send(cmd_buf, "CONNECT", 100, 5);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
        goto end;
    }
    ret = AT_Cmd_Send(post_url, "OK", 100, 5);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
        goto end;
    }

    memset(cmd_buf, 0, 30);
    sprintf(cmd_buf, "AT+QHTTPPOST=%d,%d,%d\r\n", 257, 60, 60);
    ret = AT_Cmd_Send(cmd_buf, "CONNECT", 10000, 1);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
        goto end;
    }

    //send post buff
    uart_write_bytes(UART_NUM_1, post_buf1, strlen(post_buf1));
    uart_write_bytes(UART_NUM_1, post_buf2, strlen(post_buf2));
    uart_write_bytes(UART_NUM_1, post_buf3, strlen(post_buf3));

    ret = AT_Cmd_Send("\r\n", "+QHTTPPOST: 0,200", 6000, 1);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
        goto end;
    }

    ret = AT_Cmd_Send("AT+QHTTPREAD=60\r\n", "+QHTTPREAD: 0", 100, 1);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
        goto end;
    }

end:
    free(post_url);
    free(cmd_buf);
    free(post_buf);
    if (ret == NULL)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

uint8_t EC20_Send_Post_Data(char *post_buf, bool end_flag)
{
    uart_write_bytes(EX_UART_NUM, post_buf, strlen(post_buf));
    if (end_flag == true)
    {
        if (AT_Cmd_Send("\r\n", "+QHTTPPOST: 0,200", 6000, 1) == NULL)
        {
            ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
            return 0;
        }
    }
    return 1;
}

uint8_t EC20_Read_Post_Data(char *recv_buff, uint16_t buff_size)
{
    char *rst_val;
    rst_val = AT_Cmd_Send("AT+QHTTPREAD=60\r\n", "{\"result\":", 100, 1);
    if (rst_val == NULL)
    {
        ESP_LOGE(TAG, "EC20_read %d", __LINE__);
        return 0;
    }
    memcpy(recv_buff, rst_val, buff_size);
    return 1;
}

#define API_KEY "30f29fe61a17c644315338535f91fa78"
#define C_ID "8330"
#define TOP_IC "/product/ubibot-sp1/channel/8330/control"
#define USER_ID "7926568E-2E80-41C0-9865-64FCD4123F94"
#define CMD_LEN 150

uint8_t EC20_MQTT(void)
{
    char *ret;
    char *cmd_buf;
    cmd_buf = (char *)malloc(CMD_LEN);
    memset(cmd_buf, 0, CMD_LEN);

    ret = AT_Cmd_Send("AT+QMTOPEN?\r\n", "+QMTOPEN: 0,", 6000, 5);
    if (ret != NULL)
    {
        ESP_LOGI(TAG, "EC20_MQTT already open");
        goto end;
    }

    ret = AT_Cmd_Send("AT+QMTCFG=\"version\",0,3\r\n", "OK", 100, 5);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
        goto end;
    }

    ret = AT_Cmd_Send("AT+QMTCFG=\"recv/mode\",0,0,0\r\n", "OK", 100, 5);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
        goto end;
    }

    ret = AT_Cmd_Send("AT+QMTCFG=\"keepalive\",0,120\r\n", "OK", 100, 1);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
        goto end;
    }

    ret = AT_Cmd_Send("AT+QMTOPEN=0,\"mqtt.ubibot.cn\",1883\r\n", "+QMTOPEN: 0,0", 6000, 10);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
        goto end;
    }

    sprintf(cmd_buf, "AT+QMTCONN=0,\"%s\",\"c_id=%s\",\"api_key=%s\"\r\n", USER_ID, C_ID, API_KEY);
    ret = AT_Cmd_Send(cmd_buf, "+QMTCONN: 0,0,0", 6000, 1);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
        goto end;
    }
    memset(cmd_buf, 0, CMD_LEN);
    sprintf(cmd_buf, "AT+QMTSUB=0,1,\"%s\",0\r\n", TOP_IC);
    ret = AT_Cmd_Send(cmd_buf, "+QMTSUB: ", 6000, 1);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
        goto end;
    }

end:
    // free(active_url);
    free(cmd_buf);
    if (ret == NULL)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

uint8_t EC20_MQTT_PUB(void)
{
    char *ret;
    char *cmd_buf;
    cmd_buf = (char *)malloc(CMD_LEN);
    memset(cmd_buf, 0, CMD_LEN);

    memset(cmd_buf, 0, CMD_LEN);
    sprintf(cmd_buf, "AT+QMTPUBEX=0,0,0,0,\"%s\",30\r\n", TOP_IC);
    ret = AT_Cmd_Send(cmd_buf, ">", 6000, 1);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
        goto end;
    }

    memset(cmd_buf, 0, CMD_LEN);
    sprintf(cmd_buf, "This is test data, hello MQTT.\r\n");
    ret = AT_Cmd_Send(cmd_buf, "+QMTPUBEX: 0,0,0", 6000, 1);
    if (ret == NULL)
    {
        ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
        goto end;
    }

end:
    // free(active_url);
    free(cmd_buf);
    if (ret == NULL)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}