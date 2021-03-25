#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <cJSON.h>

#include "driver/uart.h"
#include "soc/uart_periph.h"
#include "driver/gpio.h"
#include "Smartconfig.h"
#include "Led.h"
#include "Json_parse.h"
#include "Http.h"

#include "EC20.h"

#define UART1_TXD (GPIO_NUM_21)
#define UART1_RXD (GPIO_NUM_22)

static const char *TAG = "EC20";
TaskHandle_t EC20_Task_Handle;
TaskHandle_t EC20_M_Task_Handle;
TaskHandle_t Uart1_Task_Handle;
TaskHandle_t EC20_Mqtt_Handle;

#define EC20_SW 25
#define BUF_SIZE 1024
#define CMD_LEN 1024

#define EX_UART_NUM UART_NUM_1

QueueHandle_t EC_uart_queue;
QueueHandle_t EC_at_queue;
QueueHandle_t EC_ota_queue;
QueueHandle_t EC_mqtt_queue;

char ICCID[24] = {0};
uint8_t EC_RECV_MODE = EC_NORMAL;

uint32_t file_len = 0;

extern char topic_s[100];
extern char topic_p[100];
extern bool MQTT_E_STA;

bool AT_CMD_FLAG = false;
bool CHECK_FLAG = false;

void uart_event_task(void *pvParameters);
void EC20_Task(void *arg);
void EC20_M_Task(void *arg);
void EC20_Mqtt_Init_Task(void *arg);

bool EC20_Moudle_Init(void);

void REST_MOUDLE(void)
{
    gpio_set_level(EC20_SW, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(EC20_SW, 1);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    gpio_set_level(EC20_SW, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(EC20_SW, 1);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
}
//重启EC20网络初始化任务
void Res_EC20_Task(void)
{
    // xEventGroupClearBits(Net_sta_group, CONNECTED_BIT);
    // xEventGroupClearBits(Net_sta_group, ACTIVED_BIT);
    if ((xEventGroupGetBits(Net_sta_group) & EC20_Task_BIT) != EC20_Task_BIT)
    {
        xTaskCreate(EC20_Task, "EC20_Task", 3072, NULL, 9, &EC20_Task_Handle);
    }
}

//重启EC20 mqtt 初始化任务
void Res_EC20_Mqtt_Task(void)
{
    if ((xEventGroupGetBits(Net_sta_group) & EC20_M_INIT_BIT) != EC20_M_INIT_BIT)
    {
        xTaskCreate(EC20_Mqtt_Init_Task, "EC_M_Init", 2048, NULL, 6, &EC20_Mqtt_Handle);
    }
}

//开启EC20
void EC20_Start(void)
{
    if ((xEventGroupGetBits(Net_sta_group) & Uart1_Task_BIT) != Uart1_Task_BIT)
    {
        xTaskCreate(uart_event_task, "uart_event_task", 4096, NULL, 20, &Uart1_Task_Handle);
    }

    if ((xEventGroupGetBits(Net_sta_group) & EC20_M_TASK_BIT) != EC20_M_TASK_BIT)
    {
        xTaskCreate(EC20_M_Task, "EC20_M_Task", 3072, NULL, 8, &EC20_M_Task_Handle);
    }

    Res_EC20_Task();
}

//关闭EC20
// void EC20_Stop(void)
// {
//     if ((xEventGroupGetBits(Net_sta_group) & Uart1_Task_BIT) == Uart1_Task_BIT)
//     {
//         vTaskDelete(Uart1_Task_Handle);
//         xEventGroupClearBits(Net_sta_group, Uart1_Task_BIT);
//         free(EC20_RECV);
//     }

//     if ((xEventGroupGetBits(Net_sta_group) & EC20_M_TASK_BIT) == EC20_M_TASK_BIT)
//     {
//         vTaskDelete(EC20_M_Task_Handle);
//         xEventGroupClearBits(Net_sta_group, EC20_M_TASK_BIT);
//         free(mqtt_recv);
//     }
// }

void uart_event_task(void *pvParameters)
{
    xEventGroupSetBits(Net_sta_group, Uart1_Task_BIT);
    ESP_LOGI(TAG, "%d", __LINE__);
    uart_event_t event;
    uint16_t all_read_len = 0, last_all_read_len = 0;
    uint16_t header_len = 0;
    char *EC20_RECV = (char *)malloc(BUF_SIZE);
    memset(EC20_RECV, 0, BUF_SIZE);
    uart_flush(EX_UART_NUM);
    char *ret_chr = NULL;
    bool flag_1 = false;

    while (CHECK_FLAG == true || net_mode == NET_4G)
    {
        //Waiting for UART event.
        if (xQueueReceive(EC_uart_queue, (void *)&event, 10 / portTICK_PERIOD_MS))
        {
            if (ulTaskNotifyTake(pdTRUE, 0) != 0)
            {
                // uart_flush(EX_UART_NUM);
                flag_1 = false;
                all_read_len = 0;
                memset(EC20_RECV, 0, BUF_SIZE);
                // xQueueReset(EC_uart_queue);
            }
            switch (event.type)
            {
            case UART_DATA:
                // ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                if (event.size > 1)
                {
                    if (all_read_len + event.size >= BUF_SIZE)
                    {
                        ESP_LOGE(TAG, "read len flow");
                        uart_flush(EX_UART_NUM);
                        all_read_len = 0;
                        memset(EC20_RECV, 0, BUF_SIZE);
                        // xQueueReset(EC_uart_queue);
                        continue; //此处需返回循环，否则会导致循环错误
                    }
                    uart_read_bytes(EX_UART_NUM, (uint8_t *)EC20_RECV + all_read_len, event.size, 0);
                    last_all_read_len = all_read_len;
                    all_read_len += event.size;
                    EC20_RECV[all_read_len] = 0; //去掉字符串结束符，防止字符串拼接不成功
                    //分接收模式处理，为了区别OTA 读取文件差异
                    switch (EC_RECV_MODE)
                    {
                    case EC_NORMAL:
                        if (s_rstrstr(EC20_RECV, all_read_len, 3, "\r\n") != NULL)
                        {
                            xQueueOverwrite(EC_at_queue, (void *)EC20_RECV);
                            if (model_id == SIM7600)
                            {
                                if (strstr(EC20_RECV, "+CMQTTRXPAYLOAD: 0") != NULL)
                                {
                                    // ESP_LOGI("MQTT", "%s\n", EC20_RECV);
                                    if (net_mode == NET_4G)
                                    {
                                        xQueueOverwrite(EC_mqtt_queue, (void *)EC20_RECV);
                                    }
                                }
                                else if (strstr(EC20_RECV, "+CMQTTCONNLOST:") != NULL)
                                {
                                    ESP_LOGE(TAG, "%d,%s", __LINE__, EC20_RECV);
                                    Res_EC20_Task();
                                }
                            }
                            else
                            {
                                if (strstr(EC20_RECV, "+QMTRECV:") != NULL)
                                {
                                    // ESP_LOGI("MQTT", "%s\n", EC20_RECV);
                                    if (net_mode == NET_4G)
                                    {
                                        xQueueOverwrite(EC_mqtt_queue, (void *)EC20_RECV);
                                    }
                                }
                                else if (strstr(EC20_RECV, "+QMTSTAT:") != NULL)
                                {
                                    ESP_LOGE("MQTT", "%s\n", EC20_RECV);
                                    Res_EC20_Task();
                                }
                                else if (strstr(EC20_RECV, "+QMTPING:") != NULL)
                                {
                                    ESP_LOGE("MQTT", "%s\n", EC20_RECV);
                                    Res_EC20_Task();
                                }
                                // else if (strstr(EC20_RECV, "+CME ERROR:") != NULL)
                                // {
                                //     ESP_LOGE("MQTT", "%s\n", EC20_RECV);
                                //     Res_EC20_Task();
                                // }
                                // else
                                // {
                                //     xQueueOverwrite(EC_at_queue, (void *)EC20_RECV);
                                // }
                            }

                            all_read_len = 0;
                            memset(EC20_RECV, 0, BUF_SIZE);
                            uart_flush(EX_UART_NUM);
                        }
                        // else if (strstr((char *)EC20_RECV, ">") != NULL)
                        else if (EC20_RECV[all_read_len - 2] == '>')
                        {
                            xQueueOverwrite(EC_at_queue, (void *)EC20_RECV);
                            // ESP_LOGI("MQTT", "%s\n", EC20_RECV);
                            all_read_len = 0;
                            memset(EC20_RECV, 0, BUF_SIZE);
                            uart_flush(EX_UART_NUM);
                        }
                        break;

                    case EC_OTA:

                        switch (model_id)
                        {
                        case EC20:
                            ret_chr = s_rstrstr(EC20_RECV, all_read_len, 30, "\r\n\r\nOK\r\n");
                            if (ret_chr != NULL)
                            {
                                // ESP_LOGI("EC_OTA", "all_read_len=%d\n", all_read_len);
                                ret_chr = s_strstr((const char *)EC20_RECV, 7, NULL, "+QIRD:");
                                if (ret_chr != NULL)
                                {
                                    file_len = (uint32_t)strtoul(ret_chr, 0, 10);
                                    ret_chr = s_strstr(ret_chr, 7, NULL, "\r\n");
                                    header_len = (int)(ret_chr - EC20_RECV);
                                    // ESP_LOGI("EC_OTA", "file_len=%d,header_len=%d\n", file_len, header_len);
                                    if ((all_read_len - header_len - 8) >= file_len)
                                    {
                                        // ESP_LOGI("EC_OTA", "OK");
                                        xQueueOverwrite(EC_at_queue, (void *)ret_chr);
                                        all_read_len = 0;
                                        memset(EC20_RECV, 0, BUF_SIZE);
                                        uart_flush(EX_UART_NUM);
                                    }
                                }
                                // rst_val = mid(rst_val, "+QIRD: ", "\r\n", num_buff);
                            }
                            break;

                        case SIM7600:
                            // ret_chr = s_rstrstr(EC20_RECV, all_read_len, 20, "+HTTPREAD: 0");
                            // if (ret_chr != NULL)
                            // {
                            // ESP_LOGI(TAG, "%d", __LINE__);
                            //URC通知
                            if (s_strstr((const char *)EC20_RECV, 20, NULL, "+HTTP_PEER_CLOSED") != NULL)
                            {
                                ESP_LOGE(TAG, "%d", __LINE__);
                                all_read_len = last_all_read_len;
                                break;
                            }

                            if (flag_1 == false)
                            {
                                ret_chr = s_strstr((const char *)EC20_RECV, 20, NULL, "+HTTPREAD");
                                if (ret_chr != NULL)
                                {
                                    if (sscanf(ret_chr, "%*[^1-9]%d", &file_len) != 0)
                                    {
                                        ret_chr = s_strstr(ret_chr, 20, NULL, "\r\n");
                                        if (ret_chr != NULL)
                                        {
                                            flag_1 = true;
                                            header_len = (uint16_t)(ret_chr - EC20_RECV);
                                            // ESP_LOGI(TAG, "%d,all_read_len=%d,header_len=%d,file_len=%d", __LINE__, all_read_len, header_len, file_len);
                                        }
                                        else
                                        {
                                            ESP_LOGE(TAG, "%d", __LINE__);
                                        }
                                    }
                                    else
                                    {
                                        ESP_LOGE(TAG, "%d", __LINE__);
                                    }
                                }
                                else
                                {
                                    ESP_LOGE(TAG, "%d,%s", __LINE__, EC20_RECV);
                                }
                            }
                            else
                            {
                                if (all_read_len - header_len - 16 == file_len)
                                {
                                    // ESP_LOGI(TAG, "%d,header_len=%d", __LINE__, header_len);
                                    xQueueOverwrite(EC_at_queue, (void *)ret_chr);
                                    all_read_len = 0;
                                    memset(EC20_RECV, 0, BUF_SIZE);
                                }
                                else if (all_read_len - header_len - 16 > file_len)
                                {
                                    ESP_LOGI(TAG, "%d,all_read_len=%d,header_len=%d,file_len=%d", __LINE__, all_read_len, header_len, file_len);
                                    ESP_LOGI(TAG, "%s", ret_chr);
                                }
                            }

                            // }

                        default:
                            break;
                        }

                    default:
                        break;
                    }
                }
                break;

            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                uart_flush_input(EX_UART_NUM);
                uart_flush(EX_UART_NUM);
                all_read_len = 0;
                memset(EC20_RECV, 0, BUF_SIZE);
                xQueueReset(EC_uart_queue);
                break;

            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                uart_flush_input(EX_UART_NUM);
                uart_flush(EX_UART_NUM);
                all_read_len = 0;
                memset(EC20_RECV, 0, BUF_SIZE);
                xQueueReset(EC_uart_queue);
                break;

            //Others
            default:
                // ESP_LOGI(TAG, "uart type: %d", event.type);
                break;
            }
        }
    }
    xEventGroupClearBits(Net_sta_group, Uart1_Task_BIT);
    free(EC20_RECV);
    ESP_LOGI(TAG, "%d", __LINE__);
    vTaskDelete(NULL);
}

void EC20_Init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_RTS,
        .source_clk = UART_SCLK_APB,
    };
    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, 0, 5, &EC_uart_queue, 0);
    // uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
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

    gpio_set_level(EC20_SW, 1); // 1:自动开机

    // EC20_at_Binary = xSemaphoreCreateBinary();
    EC_at_queue = xQueueCreate(1, BUF_SIZE);
    EC_mqtt_queue = xQueueCreate(1, BUF_SIZE);

    if (net_mode == NET_4G)
    {
        EC20_Start();
    }
}

void EC20_Task(void *arg)
{
    xEventGroupSetBits(Net_sta_group, EC20_Task_BIT);
    uint8_t ret;
    while (net_mode == NET_4G || CHECK_FLAG == true)
    {
        ESP_LOGI(TAG, "EC20_Task START");

        REST_MOUDLE();
        MQTT_E_STA = false;
        Net_sta_flag = false;
        xEventGroupClearBits(Net_sta_group, CONNECTED_BIT);
        xEventGroupClearBits(Net_sta_group, ACTIVED_BIT);
        Start_Active();
        ESP_LOGI(TAG, "%d", __LINE__);
        ret = EC20_Moudle_Init();
        ESP_LOGI(TAG, "%d", __LINE__);
        if (ret == false)
        {
            // REST_MOUDLE();
            continue;
        }
        ret = EC20_Http_CFG();
        // if (ret == 0)
        // {
        //     continue;
        // }

        xEventGroupSetBits(Net_sta_group, CONNECTED_BIT);
        Res_EC20_Mqtt_Task();

        if (ret == 1)
        {
            break;
        }
        // REST_MOUDLE();

        // vTaskSuspend(NULL);
    }
    ESP_LOGI(TAG, "EC20_Task DELETE");
    xEventGroupClearBits(Net_sta_group, EC20_Task_BIT);
    vTaskDelete(NULL);
}

/**********************************************************/
void EC20_M_Task(void *arg)
{
    xEventGroupSetBits(Net_sta_group, EC20_M_TASK_BIT);
    ESP_LOGI(TAG, "%d", __LINE__);
    char *mqtt_recv = (char *)malloc(1024);
    memset(mqtt_recv, 0, 1024);
    while (net_mode == NET_4G)
    {
        if (xQueueReceive(EC_mqtt_queue, (void *)mqtt_recv, 10 / portTICK_PERIOD_MS) == pdPASS)
        {
            parse_objects_mqtt(mqtt_recv, true);
            memset(mqtt_recv, 0, 1024);
        }
    }

    xEventGroupClearBits(Net_sta_group, EC20_M_TASK_BIT);
    free(mqtt_recv);
    ESP_LOGI(TAG, "%d", __LINE__);
    vTaskDelete(NULL);
}

/*******************************************************************************
//Check AT Command Respon result，

*******************************************************************************/
bool AT_Cmd_Send(char *ret_buf, int start_add, uint16_t ret_len, char *cmd_buf, char *check_buff, uint32_t time_out, uint8_t try_num)
{
    xSemaphoreTake(EC20_muxtex, -1);
    bool ret = false;
    char *rst_val = NULL;
    uint32_t i, j;
    uint8_t *recv_buf;
    recv_buf = (uint8_t *)malloc(1024);

    if (Uart1_Task_Handle != NULL)
    {
        xTaskNotifyGive(Uart1_Task_Handle);
    }

    for (i = 0; i < try_num; i++)
    {
        memset(recv_buf, 0, 1024);

        if (cmd_buf != NULL)
        {
            uart_write_bytes(EX_UART_NUM, cmd_buf, strlen(cmd_buf));
        }
        if (check_buff == NULL)
        {
            break;
        }

        for (j = 0; j < (time_out / 10); j++)
        {
            if (xQueueReceive(EC_at_queue, (void *)recv_buf, 10 / portTICK_RATE_MS) == pdPASS)
            {
                rst_val = strstr((char *)recv_buf, check_buff); //
                if (rst_val != NULL)
                {
                    break;
                }
            }
            if (AT_CMD_FLAG == true)
            {
                break;
            }
        }

        if (rst_val != NULL)
        {
            ret = true;
            if (ret_buf != NULL)
            {
                memcpy(ret_buf, rst_val + start_add, ret_len);
            }
            break;
        }
        else if (AT_CMD_FLAG == true)
        {
            break;
        }
        else
        {
            //未等到数据
            ESP_LOGE(TAG, "%d,%s", __LINE__, recv_buf);
        }
    }
    AT_CMD_FLAG = false;
    // vTaskDelay(100 / portTICK_PERIOD_MS);
    free(recv_buf);
    xSemaphoreGive(EC20_muxtex);

    return ret; //
}

uint8_t EC20_Net_Check(void)
{
    uint8_t ret = 0;
    for (uint8_t i = 0; i < 15; i++)
    {
        // ESP_LOGI(TAG, "Net_Check");
        if (AT_Cmd_Send(NULL, 0, 0, "AT+QIACT?\r\n", "+QIACT: 1,1", 100, 1) != false)
        {
            ret = 1;
            break;
        }
        if (AT_Cmd_Send(NULL, 0, 0, "AT+QIACT=1\r\n", "OK", 1000, 1) == false)
        {
            ESP_LOGE(TAG, "EC20_Http_CFG %d", __LINE__);
        }
    }

    // ESP_LOGI(TAG, "Net_Check: %d", ret);
    if (ret == 0) //重启
    {
        Res_EC20_Task();
    }

    return ret;
}

//EC20 init
bool EC20_Moudle_Init(void)
{
    bool ret;
    char *cmd_buf = (char *)malloc(CMD_LEN);
    char *recv_buf = (char *)malloc(50);
    uint8_t fail_num = 0;
    int CREG_n = 0, CREG_stat = 0;
    float ec_rssi_val;

    //退出当前正在进行的AT任务
    AT_CMD_FLAG = true;

    // vTaskDelay(20000 / portTICK_PERIOD_MS);
    //重启
    while (AT_Cmd_Send(NULL, 0, 0, "AT\r\n", "OK", 500, 1) == false)
    {
        fail_num++;
        if (fail_num > 40)
        {
            Net_ErrCode = NO_ARK;
            ESP_LOGE(TAG, "%d", __LINE__);
            ret = false;
            goto end;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "%d", __LINE__);
    // ret = AT_Cmd_Send(NULL, 0, 0, "AT+CFUN=1,1\r\n", "OK", 1000, 5);
    // if (ret == false)
    // {
    //     ESP_LOGE(TAG, "%d", __LINE__);
    //     goto end;
    // }
    // vTaskDelay(30000 / portTICK_PERIOD_MS);
    // fail_num = 0;
    // while (AT_Cmd_Send(NULL, 0, 0, "AT\r\n", "OK", 100, 1) == false)
    // {
    //     fail_num++;
    //     if (fail_num > 5)
    //     {
    //         ESP_LOGE(TAG, "%d", __LINE__);
    //         ret = NULL;
    //         goto end;
    //     }
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }
    ret = AT_Cmd_Send(NULL, 0, 0, "ATE0\r\n", "OK", 100, 10); //回显
    if (ret == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        goto end;
    }

    //查询模块型号
    ret = AT_Cmd_Send(recv_buf, -20, 50, "AT+CGMM\r\n", "OK", 100, 10);
    if (ret != false)
    {
        ESP_LOGI(TAG, "%d,%s", __LINE__, recv_buf);
        if (s_strstr(recv_buf, 50, NULL, "7600"))
        {
            model_id = SIM7600;
            ESP_LOGI(TAG, "model_id = SIM7600");
        }
        else if (s_strstr(recv_buf, 50, NULL, "EC20"))
        {
            model_id = EC20;
            ESP_LOGI(TAG, "model_id = EC20");
        }
        else
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            ret = false;
            goto end;
        }
    }

    ret = AT_Cmd_Send(NULL, 0, 0, "AT+CPIN?\r\n", "READY", 500, 20);
    if (ret == false)
    {
        ESP_LOGE(TAG, "EC20_Init %d", __LINE__);
        Net_ErrCode = CPIN_ERR;
        goto end;
    }

    //AT+CICCID +ICCID: 89860439101880927899
    if (model_id == SIM7600)
    {
        ret = AT_Cmd_Send(ICCID, 8, 20, "AT+CICCID\r\n", "+ICCID:", 1000, 5);
        if (ret != false)
        {
            ESP_LOGI(TAG, "ICCID=%s", ICCID);
        }
    }
    else
    {
        ret = AT_Cmd_Send(ICCID, 8, 20, "AT+QCCID\r\n", "+QCCID:", 1000, 5);
        if (ret != false)
        {
            ESP_LOGI(TAG, "ICCID=%s", ICCID);
        }
    }

    fail_num = 0;
    while (1)
    {
        ret = EC20_Get_Rssi(&ec_rssi_val);
        if (ret != false)
        {
            ESP_LOGI(TAG, "%d,ec_rssi_val=%f", __LINE__, ec_rssi_val);
            break;
        }
        Net_ErrCode = 4100; //信号错误
        fail_num++;
        if (fail_num > 10)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            ret = false;
            goto end;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    //AT+CGREG?
    fail_num = 0;
    while (1)
    {
        ret = AT_Cmd_Send(recv_buf, 0, 50, "AT+CGREG?\r\n", "+CGREG:", 500, 10);
        if (ret != false)
        {
            sscanf(recv_buf, "%*[^: ]: %d,%d", &CREG_n, &CREG_stat);
            ESP_LOGI(TAG, "%d,%s,CREG_n=%d,CREG_stat=%d", __LINE__, recv_buf, CREG_n, CREG_stat);
            if (CREG_n == 0 && (CREG_stat == 1 || CREG_stat == 5))
            {
                break;
            }
        }
        Net_ErrCode = 4000 + CREG_n * 10 + CREG_stat;
        fail_num++;
        if (fail_num > 10)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            ret = false;
            goto end;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    if (model_id == SIM7600)
    {
        snprintf(cmd_buf, CMD_LEN, "AT+CGDCONT=1,\"IP\",\"%s\"\r\n", SIM_APN);
        ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, "OK", 1000, 5);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_Init %d", __LINE__);
            Net_ErrCode = QICSGP_ERR;
            goto end;
        }
        //设置user
        if (strlen(SIM_USER) > 0)
        {
            snprintf(cmd_buf, CMD_LEN, "AT+CGAUTH=1,1,\"%s\",\"%s\"\r\n", SIM_PWD, SIM_USER);
            ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, "OK", 1000, 5);
            if (ret == false)
            {
                ESP_LOGE(TAG, "EC20_Init %d,%s", __LINE__, cmd_buf);
                Net_ErrCode = QICSGP_ERR;
                goto end;
            }
        }
    }
    else
    {
        snprintf(cmd_buf, CMD_LEN, "AT+QICSGP=1,1,\"%s\",\"%s\",\"%s\",1\r\n", SIM_APN, SIM_USER, SIM_PWD);
        ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, "OK", 1000, 5);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_Init %d", __LINE__);
            Net_ErrCode = QICSGP_ERR;
            goto end;
        }
    }

    //OK
    // ret = AT_Cmd_Send(NULL, 0, 0, "AT+CGATT=1\r\n", "OK", 1000, 5);
    // if (ret == false)
    // {
    //     ESP_LOGE(TAG, "EC20_Init %d", __LINE__);
    //     Net_ErrCode = CGATT_ERR;
    //     goto end;
    // }
    // //OK
    // ret = AT_Cmd_Send(NULL, 0, 0, "AT+CGATT?\r\n", "+CGATT: 1", 1000, 20);
    // if (ret == false)
    // {
    //     ESP_LOGE(TAG, "EC20_Init %d", __LINE__);
    //     Net_ErrCode = CGATT_ERR;
    //     goto end;
    // }

    // if (model_id == SIM7600)
    // {
    //     ret = AT_Cmd_Send(NULL, 0, 0, "AT+CGACT?\r\n", "+CGACT: 1,1", 100, 10);
    //     if (ret != false)
    //     {
    //         // ESP_LOGI(TAG, "EC20_Http_CFG %d", __LINE__);
    //         goto end;
    //     }

    //     //AT+CGACT=1,1
    //     //OK
    //     ret = AT_Cmd_Send(NULL, 0, 0, "AT+CGACT=1,1\r\n", "OK", 100, 50);
    //     if (ret == false)
    //     {
    //         ESP_LOGE(TAG, "EC20_Http_CFG %d", __LINE__);
    //         Net_ErrCode = QIACT_ERR;
    //         goto end;
    //     }

    //     // AT+CGACT?
    //     // +CGACT: 1,1
    //     ret = AT_Cmd_Send(NULL, 0, 0, "AT+CGACT?\r\n", "+CGACT: 1,1", 100, 100);
    //     if (ret != false)
    //     {
    //         // ESP_LOGI(TAG, "EC20_Http_CFG %d", __LINE__);
    //         goto end;
    //     }
    // }
    // else
    // {
    //     ret = AT_Cmd_Send(NULL, 0, 0, "AT+QIACT?\r\n", "+QIACT: 1,1", 1000, 10);
    //     if (ret != false)
    //     {
    //         // ESP_LOGI(TAG, "EC20_Http_CFG %d", __LINE__);
    //         goto end;
    //     }
    //     //AT+CGACT=1,1
    //     //OK
    //     ret = AT_Cmd_Send(NULL, 0, 0, "AT+QIACT=1\r\n", "OK", 1000, 10);
    //     if (ret == false)
    //     {
    //         ESP_LOGE(TAG, "EC20_Http_CFG %d", __LINE__);
    //         Net_ErrCode = QIACT_ERR;
    //         goto end;
    //     }

    //     //AT+CGACT?
    //     //+CGACT: 1,1
    //     ret = AT_Cmd_Send(NULL, 0, 0, "AT+QIACT?\r\n", "+QIACT: 1,1", 100, 10);
    //     if (ret != false)
    //     {
    //         // ESP_LOGI(TAG, "EC20_Http_CFG %d", __LINE__);
    //         goto end;
    //     }
    // }

end:
    free(recv_buf);
    free(cmd_buf);
    return ret;
}

//EC20 MQTT INIT
void EC20_Mqtt_Init_Task(void *arg)
{
    xEventGroupSetBits(Net_sta_group, EC20_M_INIT_BIT);
    uint8_t ret;
    while (1)
    {
        xEventGroupWaitBits(Net_sta_group, ACTIVED_BIT | MQTT_INITED_BIT, false, true, -1); //等待激活
        if (net_mode != NET_4G)
        {
            MQTT_E_STA = false;
            break;
        }

        xSemaphoreTake(xMutex_Http_Send, -1);
        ret = EC20_MQTT_INIT();
        xSemaphoreGive(xMutex_Http_Send);
        if (ret == 0)
        {
            Res_EC20_Task();
        }
        else
        {
            MQTT_E_STA = true;
            break;
        }
    }
    xEventGroupClearBits(Net_sta_group, EC20_M_INIT_BIT);
    vTaskDelete(NULL);
}

uint8_t EC20_Http_CFG(void)
{
    bool ret;

    if (model_id == SIM7600)
    {
        ret = AT_Cmd_Send(NULL, 0, 0, "AT+HTTPINIT\r\n", "OK", 1000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "HTTPINIT %d", __LINE__);
            goto end;
        }
    }
    else
    {
        ret = AT_Cmd_Send(NULL, 0, 0, "AT+QHTTPCFG=\"contextid\",1\r\n", "OK", 1000, 5);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_Http_CFG %d", __LINE__);
            goto end;
        }

        ret = AT_Cmd_Send(NULL, 0, 0, "AT+QHTTPCFG=\"responseheader\",0\r\n", "OK", 1000, 5);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_Http_CFG %d", __LINE__);
            goto end;
        }
    }

end:
    // free(active_url);

    return ret;
}

uint8_t EC20_Active(char *active_url, char *recv_buf)
{
    bool ret;
    char *cmd_buf;
    uint8_t active_len;
    cmd_buf = (char *)malloc(CMD_LEN);
    memset(cmd_buf, 0, CMD_LEN);

    if (model_id == SIM7600)
    {
        snprintf(cmd_buf, CMD_LEN, "AT+HTTPPARA=\"URL\",\"%s\"\r\n", active_url);
        // ESP_LOGI(TAG, "%s", cmd_buf);
        ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, "OK", 1000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_Active %d", __LINE__);
            goto end;
        }

        ret = AT_Cmd_Send(NULL, 0, 0, "AT+HTTPACTION=0\r\n", "+HTTPACTION: 0,200", 1000, 5);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_Active %d", __LINE__);
            goto end;
        }
        // ESP_LOGI(TAG, "%d\n,%s", __LINE__, ret);

        ret = AT_Cmd_Send(recv_buf, 0, BUF_SIZE, "AT+HTTPREAD=0,2048\r\n", "+HTTPREAD", 1000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_Active %d", __LINE__);
            goto end;
        }
        // ESP_LOGI(TAG, "%d\n,%s", __LINE__, ret);
    }
    else
    {
        active_len = strlen(active_url); //不包含换行符
        snprintf(cmd_buf, CMD_LEN, "AT+QHTTPURL=%d,60\r\n", active_len);
        ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, "CONNECT", 1000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_Active %d", __LINE__);
            goto end;
        }

        memset(cmd_buf, 0, CMD_LEN);
        snprintf(cmd_buf, CMD_LEN, "%s\r\n", active_url);
        ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, "OK", 60000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_Active %d", __LINE__);
            goto end;
        }

        ret = AT_Cmd_Send(NULL, 0, 0, "AT+QHTTPGET=60\r\n", "+QHTTPGET: 0,200", 6000, 1);
        if (ret == false)
        {
            // Res_EC20_Task();
            ESP_LOGE(TAG, "EC20_Active %d", __LINE__);
            goto end;
        }

        ret = AT_Cmd_Send(recv_buf, 0, BUF_SIZE, "AT+QHTTPREAD=60\r\n", "CONNECT", 6000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_Active %d", __LINE__);
            goto end;
        }
    }

end:
    free(cmd_buf);
    if (ret == false)
    {
        Res_EC20_Task();
    }
    return ret;
}

uint8_t EC20_Send_Post_Data(char *post_buf, bool end_flag)
{
    bool ret;

    if (model_id == SIM7600)
    {
        if (end_flag != true)
        {
            AT_Cmd_Send(NULL, 0, 0, post_buf, NULL, 1, 1);
        }
        else
        {
            if (AT_Cmd_Send(NULL, 0, 0, post_buf, "OK", 5000, 1) == false)
            {
                ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
                // Res_EC20_Task();
                return 0;
            }
            //发送
            if ((ret = AT_Cmd_Send(NULL, 0, 0, "AT+HTTPACTION=1\r\n", "+HTTPACTION: 1,200", 5000, 2)) == false)
            {
                ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
                // Res_EC20_Task();
                return 0;
            }
        }
    }
    else
    {
        if (end_flag != true)
        {
            AT_Cmd_Send(NULL, 0, 0, post_buf, NULL, 1, 1);
        }
        else
        {
            AT_Cmd_Send(NULL, 0, 0, post_buf, NULL, 1, 1);
            if (AT_Cmd_Send(NULL, 0, 0, "\r\n", "+QHTTPPOST: 0,200", 5000, 1) == false)
            {
                ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
                // Res_EC20_Task();
                return 0;
            }
        }
    }

    return 1;
}

uint8_t EC20_Read_Post_Data(char *recv_buff, uint16_t buff_size)
{
    bool rst_val;

    if (model_id == SIM7600)
    {
        char *cmd_buf;
        cmd_buf = (char *)malloc(CMD_LEN);
        memset(cmd_buf, 0, CMD_LEN);
        snprintf(cmd_buf, CMD_LEN, "AT+HTTPREAD=0,%d\r\n", buff_size);
        rst_val = AT_Cmd_Send(recv_buff, 0, buff_size, cmd_buf, "+HTTPREAD", 10000, 1);
        free(cmd_buf);
        // rst_val = AT_Cmd_Send("AT+HTTPREAD=0,2048\r\n", "OK", 1000, 1);
        if (rst_val == false)
        {
            ESP_LOGE(TAG, "EC20_read %d", __LINE__);
        }
    }
    else
    {
        rst_val = AT_Cmd_Send(recv_buff, 0, buff_size, "AT+QHTTPREAD=60\r\n", "CONNECT", 5000, 2);
        if (rst_val == false)
        {
            ESP_LOGE(TAG, "EC20_read %d", __LINE__);
        }
    }

    return rst_val;
}

uint8_t EC20_MQTT_INIT(void)
{
    bool ret;
    char *cmd_buf;
    cmd_buf = (char *)malloc(CMD_LEN);

    if (model_id == SIM7600)
    {
        //开启mqtt
        ret = AT_Cmd_Send(NULL, 0, 0, "AT+CMQTTSTART\r\n", "OK", 1000, 1);
        ret = AT_Cmd_Send(NULL, 0, 0, "AT+CMQTTACCQ=0,\"client1\"\r\n", "OK", 5000, 1);

        //连接
        memset(cmd_buf, 0, CMD_LEN);
        snprintf(cmd_buf, CMD_LEN, "AT+CMQTTCONNECT=0,\"tcp://%s:%s\",60,1,\"c_id=%s\",\"api_key=%s\"\r\n",
                 MQTT_SERVER,
                 MQTT_PORT,
                 ChannelId,
                 ApiKey);
        // ESP_LOGI(TAG, "%d,%s", __LINE__, cmd_buf);
        ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, "+CMQTTCONNECT: 0,0", 10000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            goto end;
        }
        //ESP_LOGI(TAG, "%d", __LINE__);

        //订阅
        memset(cmd_buf, 0, CMD_LEN);
        snprintf(cmd_buf, CMD_LEN, "AT+CMQTTSUB=0,%d,1\r\n", strlen(topic_s));
        // ESP_LOGI(TAG, "%d,%s", __LINE__, cmd_buf);
        ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, ">", 1000, 5);
        if (ret == false)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            goto end;
        }
        //ESP_LOGI(TAG, "%d", __LINE__);
        ret = AT_Cmd_Send(NULL, 0, 0, topic_s, "+CMQTTSUB: 0,0", 5000, 2);
        if (ret == false)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            goto end;
        }
        //ESP_LOGI(TAG, "%d", __LINE__);
    }
    else
    {
        ret = AT_Cmd_Send(NULL, 0, 0, "AT+QMTOPEN?\r\n", "+QMTOPEN: 0,", 1000, 5);
        if (ret != false)
        {
            ESP_LOGI(TAG, "EC20_MQTT already open");
            goto end;
        }

        ret = AT_Cmd_Send(NULL, 0, 0, "AT+QMTCFG=\"version\",0,3\r\n", "OK", 1000, 5);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
            goto end;
        }

        ret = AT_Cmd_Send(NULL, 0, 0, "AT+QMTCFG=\"recv/mode\",0,0,0\r\n", "OK", 1000, 5);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
            goto end;
        }

        ret = AT_Cmd_Send(NULL, 0, 0, "AT+QMTCFG=\"keepalive\",0,120\r\n", "OK", 1000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
            goto end;
        }

        memset(cmd_buf, 0, CMD_LEN);
        snprintf(cmd_buf, CMD_LEN, "AT+QMTOPEN=0,\"%s\",%s\r\n", MQTT_SERVER, MQTT_PORT);
        ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, "+QMTOPEN: 0,0", 6000, 10);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
            goto end;
        }

        memset(cmd_buf, 0, CMD_LEN);
        snprintf(cmd_buf, CMD_LEN, "AT+QMTCONN=0,\"%s\",\"c_id=%s\",\"api_key=%s\"\r\n", USER_ID, ChannelId, ApiKey);
        ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, "+QMTCONN: 0,0,0", 6000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
            goto end;
        }

        memset(cmd_buf, 0, CMD_LEN);
        snprintf(cmd_buf, CMD_LEN, "AT+QMTSUB=0,1,\"%s\",0\r\n", topic_s);
        ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, "+QMTSUB: ", 6000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
            goto end;
        }
    }

end:
    // free(active_url);
    free(cmd_buf);
    return ret;
}

//data_buff 需要包含 \r\n
uint8_t EC20_MQTT_PUB(char *data_buff)
{
    bool ret;
    char *cmd_buf;
    cmd_buf = (char *)malloc(CMD_LEN);
    memset(cmd_buf, 0, CMD_LEN);
    // ESP_LOGI(TAG, "MQTT LEN:%d,\n%s", strlen(data_buff), data_buff);

    if (model_id == SIM7600)
    {
        //设置发布主题
        memset(cmd_buf, 0, CMD_LEN);
        snprintf(cmd_buf, CMD_LEN, "AT+CMQTTTOPIC=0,%d\r\n", strlen(topic_p));
        ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, ">", 1000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            goto end;
        }
        //ESP_LOGI(TAG, "%d", __LINE__);

        ret = AT_Cmd_Send(NULL, 0, 0, topic_p, "OK", 1000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            goto end;
        }
        //ESP_LOGI(TAG, "%d", __LINE__);

        memset(cmd_buf, 0, CMD_LEN);
        snprintf(cmd_buf, CMD_LEN, "AT+CMQTTPAYLOAD=0,%d\r\n", strlen(data_buff) - 2);
        ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, ">", 1000, 5);
        if (ret == false)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            goto end;
        }
        //ESP_LOGI(TAG, "%d", __LINE__);

        ret = AT_Cmd_Send(NULL, 0, 0, data_buff, "OK", 1000, 5);
        if (ret == false)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            goto end;
        }
        //ESP_LOGI(TAG, "%d", __LINE__);

        ret = AT_Cmd_Send(NULL, 0, 0, "AT+CMQTTPUB=0,1,60\r\n", "+CMQTTPUB: 0,0", 1000, 5);
        if (ret == false)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            goto end;
        }
        //ESP_LOGI(TAG, "%d", __LINE__);
    }
    else
    {
        snprintf(cmd_buf, CMD_LEN, "AT+QMTPUBEX=0,0,0,0,\"%s\",%d\r\n", topic_p, strlen(data_buff) - 2);
        ret = AT_Cmd_Send(NULL, 0, 0, cmd_buf, ">", 1000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
            goto end;
        }

        ret = AT_Cmd_Send(NULL, 0, 0, data_buff, "+QMTPUBEX: 0,0,0", 1000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, "EC20_MQTT %d", __LINE__);
            goto end;
        }
    }

end:
    // free(active_url);
    free(cmd_buf);
    if (ret == false)
    {
        Res_EC20_Task();
    }
    return ret;
}

//获取信号值
bool EC20_Get_Rssi(float *Rssi_val)
{
    xSemaphoreTake(xMutex_Http_Send, -1);
    int csq_num;
    uint8_t fail_num = 0;
    bool ret_val;
    char *temp_buf;
    // char *InpString;
    temp_buf = (char *)malloc(15);
    memset(temp_buf, 0, 15);
    while (1)
    {
        fail_num++;
        if (fail_num > 10)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            ret_val = false;
            break;
        }
        //+CSQ: 24,5
        if (AT_Cmd_Send(temp_buf, -15, 15, "AT+CSQ\r\n", "OK", 100, 1) == false)
        {
            continue;
        }
        ESP_LOGI(TAG, "%d,%s", __LINE__, temp_buf);
        sscanf(temp_buf, "%*[^: ]: %d,%*d", &csq_num);
        // InpString = strtok(temp_buf, ":");
        // InpString = strtok(NULL, ",");
        // csq_num = (uint8_t)strtoul(InpString, 0, 10);
        ESP_LOGI(TAG, "%d,CSQ:%d", __LINE__, csq_num);
        if (csq_num > 0 && csq_num <= 31)
        {
            *Rssi_val = csq_num * 2 - 113; //GPRS Signal Quality
            ret_val = true;
            break;
        }
        continue;
    }

    free(temp_buf);
    xSemaphoreGive(xMutex_Http_Send);

    return ret_val;
}

bool End_EC_TCP_OTA(void)
{
    bool ret;
    if (model_id == SIM7600)
    {
        Res_EC20_Mqtt_Task();
        return true;
    }
    else
    {
        Res_EC20_Mqtt_Task();
        ret = AT_Cmd_Send(NULL, 0, 0, "AT+QICLOSE=0\r\n", "OK", 1000, 1);
        if (ret == false)
        {
            ESP_LOGE(TAG, " %d", __LINE__);
            return false;
        }
        return true;
    }
}

//tcp 模式OTA
bool Start_EC20_TCP_OTA(void)
{
    bool rst_val = false;
    char *cmd_buf = (char *)malloc(CMD_LEN);
    // char *get_buf = (char *)malloc(256);
    char *host_buf = (char *)malloc(128);
    char *get_data = NULL;
    // uint8_t *recv_buf = (uint8_t *)malloc(BUF_SIZE);

    //获取HOST
    memset(host_buf, 0, 128);
    if ((get_data = mid(mqtt_json_s.mqtt_ota_url, "://", "/", host_buf)) == NULL)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        goto end;
    }
    ESP_LOGI(TAG, "host:%s,len:%d", host_buf, strlen(host_buf));

    //关闭MQTT
    rst_val = AT_Cmd_Send(NULL, 0, 0, "AT+QMTCLOSE=0\r\n", "+QMTCLOSE:", 1000, 1);
    if (rst_val == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        goto end;
    }

    //建立TCP连接
    snprintf(cmd_buf, CMD_LEN, "AT+QIOPEN=1,0,\"TCP\",\"%s\",80,0,0\r\n", host_buf);
    rst_val = AT_Cmd_Send(NULL, 0, 0, cmd_buf, "+QIOPEN: 0,0", 5000, 1);
    if (rst_val == false)
    {
        ESP_LOGE(TAG, " %d", __LINE__);
        goto end;
    }

    snprintf(cmd_buf, CMD_LEN, "AT+QISEND=0,%d\r\n", (24 + strlen(get_data) + strlen(host_buf)));
    rst_val = AT_Cmd_Send(NULL, 0, 0, cmd_buf, ">", 1000, 1);
    if (rst_val == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        goto end;
    }

    snprintf(cmd_buf, CMD_LEN, "GET %s HTTP/1.1\r\nHost:%s\r\n\r\n", get_data, host_buf);
    ESP_LOGI(TAG, "cmd_buf:\n%s", cmd_buf);
    rst_val = AT_Cmd_Send(NULL, 0, 0, cmd_buf, "+QIURC: \"recv\"", 6000, 1);
    if (rst_val == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        goto end;
    }

end:
    free(cmd_buf);
    free(host_buf);

    return rst_val;
}

//读取升级文件 tcp
uint32_t Read_OTA_File(char *file_buff)
{
    char *cmd_buf = (char *)malloc(CMD_LEN);
    // uint8_t *recv_buf = (uint8_t *)malloc(BUF_SIZE);
    // char *rst_val = NULL;
    // char num_buff[5] = {0};
    uint32_t ret = 0;

    xSemaphoreTake(EC20_muxtex, -1);
    EC_RECV_MODE = EC_OTA;

    if (model_id == EC20)
    {
        snprintf(cmd_buf, CMD_LEN, "AT+QIRD=0,%d\r\n", BUF_SIZE - 25);
    }
    else
    {
        snprintf(cmd_buf, CMD_LEN, "AT+HTTPREAD=0,%d\r\n", BUF_SIZE - 100);
    }

    if (Uart1_Task_Handle != NULL)
    {
        xTaskNotifyGive(Uart1_Task_Handle);
    }

    uart_write_bytes(EX_UART_NUM, cmd_buf, strlen(cmd_buf));

    if (xQueueReceive(EC_at_queue, (void *)file_buff, 20000 / portTICK_RATE_MS) == pdPASS)
    {
        ret = file_len;
        // ESP_LOGI(TAG, "%d,file_len=%d,%s", __LINE__, file_len, file_buff);
    }
    else //未等到数据
    {
        ESP_LOGE(TAG, "LINE %d", __LINE__);
        ret = 0;
    }

    EC_RECV_MODE = EC_NORMAL;
    xSemaphoreGive(EC20_muxtex);

    free(cmd_buf);
    // free(recv_buf);
    return ret;
}
//
uint32_t Start_SIM_OTA(void)
{
    bool ret;
    uint32_t content_len = 0;
    char *temp_buf = (char *)malloc(CMD_LEN);

    //ESP_LOGI(TAG, "%d", __LINE__);
    ret = AT_Cmd_Send(NULL, 0, 0, "AT+CMQTTDISC=0,120\r\n", "+CMQTTDISC: 0", 5000, 2);
    if (ret == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
    }
    ret = AT_Cmd_Send(NULL, 0, 0, "AT+CMQTTREL=0\r\n", "OK", 5000, 2);
    if (ret == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
    }
    ret = AT_Cmd_Send(NULL, 0, 0, "AT+CMQTTSTOP\r\n", "OK", 5000, 2);
    if (ret == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
    }

    memset(temp_buf, 0, CMD_LEN);
    snprintf(temp_buf, CMD_LEN, "AT+HTTPPARA=\"URL\",\"%s\"\r\n", mqtt_json_s.mqtt_ota_url);
    // ESP_LOGI(TAG, "%s", cmd_buf);
    ret = AT_Cmd_Send(NULL, 0, 0, temp_buf, "OK", 1000, 1);
    if (ret == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        goto end;
    }
    memset(temp_buf, 0, CMD_LEN);
    ret = AT_Cmd_Send(temp_buf, 0, CMD_LEN, "AT+HTTPACTION=0\r\n", "+HTTPACTION: 0,200", 6000, 1);
    if (ret == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        goto end;
    }
    ESP_LOGI(TAG, "%d\n,%s", __LINE__, temp_buf);
    sscanf(temp_buf, "%*[^,],%*d,%d", &content_len);

end:
    free(temp_buf);
    return content_len;
}

//读取升级文件 http
// uint32_t Read_HTTP_OTA_File(char *file_buff)
// {
//     char *ret;
//     ret = AT_Cmd_Send("AT+HTTPREAD=0,800\r\n", "+HTTPREAD", 5000, 1);
//     if (ret == NULL)
//     {
//         ESP_LOGE(TAG, "%d", __LINE__);
//         file_len = 0;
//         goto end;
//     }
//     // ESP_LOGI(TAG, "%d\n,%s", __LINE__, ret);
//     sscanf(ret, "%*[^1-9]%d", &file_len);
//     // ESP_LOGI(TAG, "%d\n,file_len:%d", __LINE__, file_len);
//     ret = strstr(ret, "\r\n");
//     memcpy(file_buff, (ret + 2), file_len);

// end:
//     return file_len;
// }

// 检查模块硬件
void Check_Module(void)
{
    bool ret = false;
    bool module_flag = false;
    bool simcard_flag = false;
    bool result_flag = false;
    uint8_t fail_num = 0;
    float ec_rssi_val;

    CHECK_FLAG = true;
    if ((xEventGroupGetBits(Net_sta_group) & Uart1_Task_BIT) != Uart1_Task_BIT)
    {
        xTaskCreate(uart_event_task, "uart_event_task", 5120, NULL, 20, &Uart1_Task_Handle);
    }

    cJSON *root = cJSON_CreateObject();
    char *json_temp;

    xSemaphoreTake(xMutex_Http_Send, -1);
    //重启
    while (AT_Cmd_Send(NULL, 0, 0, "AT\r\n", "OK", 100, 1) == false)
    {
        fail_num++;
        if (fail_num > 40)
        {
            Net_ErrCode = NO_ARK;
            ESP_LOGE(TAG, "%d", __LINE__);
            Res_EC20_Task();
            xSemaphoreGive(xMutex_Http_Send);
            goto end;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    module_flag = true;

    ret = AT_Cmd_Send(NULL, 0, 0, "AT+CPIN?\r\n", "READY", 100, 50);
    if (ret == false)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        simcard_flag = false;
        result_flag = false;
        xSemaphoreGive(xMutex_Http_Send);
        goto end;
    }

    xSemaphoreGive(xMutex_Http_Send);

    if (EC20_Get_Rssi(&ec_rssi_val))
    {
        simcard_flag = true;
        result_flag = true;
    }

end:
    CHECK_FLAG = false;

    if (result_flag == true)
    {
        cJSON_AddStringToObject(root, "result", "OK");
    }
    else
    {
        cJSON_AddStringToObject(root, "result", "ERROR");
    }

    if (module_flag == true)
    {
        cJSON_AddStringToObject(root, "module", "OK");
    }
    else
    {
        cJSON_AddStringToObject(root, "module", "ERROR");
    }

    if (simcard_flag == true)
    {
        cJSON_AddStringToObject(root, "simcard", "OK");
        cJSON_AddNumberToObject(root, "RSSI", ec_rssi_val);
    }
    else
    {
        cJSON_AddStringToObject(root, "simcard", "ERROR");
    }

    json_temp = cJSON_PrintUnformatted(root);
    if (json_temp != NULL)
    {
        printf("%s\r\n", json_temp);
        cJSON_free(json_temp);
    }
    //串口响应

    cJSON_Delete(root); //delete pJson
}

// int helperParseCommand(char buffer[],
//                        char stringArray[MAX_COMMAND_ARGUMENTS][MAX_COMMAND_ARGUMENT_LENGTH])
// {
//     char delimiters[] = " ,:/+\"\r\n";
//     char *token = strtok(buffer, delimiters);
//     int count = 0;
//     while (token != NULL && count < MAX_COMMAND_ARGUMENTS)
//     {
//         strncpy(stringArray[count++], token, MAX_COMMAND_ARGUMENT_LENGTH);
//         token = strtok(NULL, delimiters);
//     }
//     return count;
// }