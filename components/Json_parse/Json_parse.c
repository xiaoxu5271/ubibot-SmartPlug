#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include "esp_system.h"
#include "esp_log.h"
#include "Json_parse.h"
#include "Nvs.h"
#include "ServerTimer.h"
#include "Http.h"

#include "esp_wifi.h"
#include "Smartconfig.h"
#include "E2prom.h"
#include "Http.h"
#include "Cache_data.h"
//#include "Motorctl.h"
//#include "Wind.h"
#include "Bluetooth.h"
// #include "Human.h"
// #include "sht30dis.h"
#include "ota.h"
#include "Led.h"
// #include "w5500_driver.h"
#include "tcp_bsp.h"
#include "my_base64.h"
// #include "Human.h"
#include "RS485_Read.h"
#include "ds18b20.h"
#include "RtcUsr.h"
#include "CSE7759B.h"
#include "Switch.h"

//wifi_config_t wifi_config;

//#define DEBUG_
// uint8_t fn_set_flag = 0;     //metadata 读取标志，未读取则使用下面固定参数
uint32_t fn_dp = 0;        //数据发送频率
uint32_t fn_485_th = 0;    //485温湿度探头
uint32_t fn_485_sth = 0;   //485 土壤探头
uint32_t fn_ext = 0;       //18b20
uint32_t fn_energy = 0;    //电能信息：电压/电流/功率
uint32_t fn_ele_quan = 0;  //用电量统计
uint8_t cg_data_led = 1;   //发送数据 LED状态 0关闭，1打开
uint8_t net_mode = 0;      //上网模式选择 0：自动模式 1：lan模式 2：wifi模式
uint8_t de_switch_sta = 0; //开关默认上电状态

typedef enum
{
    HTTP_JSON_FORMAT_TINGERROR,
    HTTP_RESULT_UNSUCCESS,
    HTTP_OVER_PARSE,
    HTTP_WRITE_FLASH_OVER,
    CREAT_OBJECT_OVER,
} http_error_info;

extern uint32_t HTTP_STATUS;
char get_num[2];
char *key;
int n = 0, i;

static short Parse_metadata(char *ptrptr)
{
    if (NULL == ptrptr)
    {
        return 0;
    }

    cJSON *pJsonJson = cJSON_Parse(ptrptr);
    if (NULL == pJsonJson)
    {
        cJSON_Delete(pJsonJson); //delete pJson
        return 0;
    }
    cJSON *pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_dp"); //"fn_dp"
    if (NULL != pSubSubSub)
    {

        if ((unsigned long)pSubSubSub->valueint != fn_dp)
        {
            fn_dp = (unsigned long)pSubSubSub->valueint;
            AT24CXX_WriteLenByte(fn_dp_add, fn_dp, 4);
            printf("fn_dp = %d\n", fn_dp);
            // if (timer_human_handle != NULL)
            // {
            //     human_intr_num = 0;
            //     if (fn_dp > 0)
            //     {
            //         esp_timer_stop(timer_human_handle);
            //         esp_timer_start_periodic(timer_human_handle, fn_dp * 1000000); //创建定时器，单位us
            //         printf("start human send timer!\n");
            //     }
            //     else
            //     {
            //         esp_timer_stop(timer_human_handle);
            //         printf("stop human send timer!\n");
            //     }
            // }
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_485_th"); //"fn_th"
    if (NULL != pSubSubSub)
    {

        if ((uint32_t)pSubSubSub->valueint != fn_485_th)
        {
            fn_485_th = (uint32_t)pSubSubSub->valueint;
            AT24CXX_WriteLenByte(fn_485_th_add, fn_485_th, 4);
            printf("fn_485_th = %d\n", fn_485_th);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_485_sth"); //"fn_485_sth"
    if (NULL != pSubSubSub)
    {
        if ((uint32_t)pSubSubSub->valueint != fn_485_sth)
        {
            fn_485_sth = (uint32_t)pSubSubSub->valueint;
            AT24CXX_WriteLenByte(fn_485_sth_add, fn_485_sth, 4);
            printf("fn_485_sth = %d\n", fn_485_sth);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_ext"); //"fn_ext"
    if (NULL != pSubSubSub)
    {
        if ((uint32_t)pSubSubSub->valueint != fn_ext)
        {
            fn_ext = (uint32_t)pSubSubSub->valueint;
            AT24CXX_WriteLenByte(fn_ext_add, fn_ext, 4);
            printf("fn_ext = %d\n", fn_ext);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_energy"); //"fn_energy"
    if (NULL != pSubSubSub)
    {
        if ((uint32_t)pSubSubSub->valueint != fn_energy)
        {
            fn_energy = (uint32_t)pSubSubSub->valueint;
            AT24CXX_WriteLenByte(fn_energy_add, fn_energy, 4);
            printf("fn_energy = %d\n", fn_energy);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_ele_quan"); //"fn_ele_quan"
    if (NULL != pSubSubSub)
    {
        if ((uint32_t)pSubSubSub->valueint != fn_ele_quan)
        {
            fn_ele_quan = (uint32_t)pSubSubSub->valueint;
            AT24CXX_WriteLenByte(fn_ele_quan_add, fn_ele_quan, 4);
            printf("fn_ele_quan = %d\n", fn_ele_quan);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "cg_data_led"); //"cg_data_led"
    if (NULL != pSubSubSub)
    {

        if ((uint8_t)pSubSubSub->valueint != cg_data_led)
        {
            cg_data_led = (uint8_t)pSubSubSub->valueint;
            AT24CXX_WriteOneByte(cg_data_led_add, cg_data_led);
            printf("cg_data_led = %d\n", cg_data_led);
            if (cg_data_led == 0)
            {
                Turn_Off_LED();
            }
            else
            {
                Turn_ON_LED();
            }
        }
    }

    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "net_mode"); //"net_mode"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != net_mode)
        {
            net_mode = (uint8_t)pSubSubSub->valueint;
            AT24CXX_WriteOneByte(net_mode_add, net_mode); //写入net_mode
            printf("net_mode = %d\n", net_mode);
        }
    }
    cJSON_Delete(pJsonJson);
    // fn_set_flag = AT24CXX_ReadOneByte(fn_set_flag_add);
    // if (fn_set_flag != 1)
    // {
    //     fn_set_flag = 1;
    //     AT24CXX_WriteOneByte(fn_set_flag_add, fn_set_flag);
    // }

    return 1;
}

//解析蓝牙数据包
int32_t parse_objects_bluetooth(char *blu_json_data)
{
    cJSON *cjson_blu_data_parse = NULL;
    cJSON *cjson_blu_data_parse_command = NULL;
    // cJSON *cjson_blu_data_parse_wifissid = NULL;
    // cJSON *cjson_blu_data_parse_wifipwd = NULL;
    // cJSON *cjson_blu_data_parse_ob = NULL;
    //cJSON *cjson_blu_data_parse_devicepwd = NULL;

    printf("start_ble_parse_json：\r\n%s\n", blu_json_data);
    if (blu_json_data[0] != '{')
    {
        printf("blu_json_data Json Formatting error\n");
        return 0;
    }

    cjson_blu_data_parse = cJSON_Parse(blu_json_data);
    if (cjson_blu_data_parse == NULL) //如果数据包不为JSON则退出
    {
        printf("Json Formatting error2\n");
        cJSON_Delete(cjson_blu_data_parse);
        return BLU_JSON_FORMAT_ERROR;
    }
    else
    {
        cjson_blu_data_parse_command = cJSON_GetObjectItem(cjson_blu_data_parse, "command");
        printf("command=%s\r\n", cjson_blu_data_parse_command->valuestring);

        ParseTcpUartCmd(cJSON_Print(cjson_blu_data_parse));
    }
    cJSON_Delete(cjson_blu_data_parse);

    if (xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, 10000 / portTICK_RATE_MS))
    {
        return http_activate();
    }
    else
    {
        // if (net_mode == NET_WIFI)
        {
            return Wifi_ErrCode;
        }

        // else
        // {
        //     return LAN_ERR_CODE;
        // }
    }
}

//解析激活返回数据
esp_err_t parse_objects_http_active(char *http_json_data)
{
    cJSON *json_data_parse = NULL;
    cJSON *json_data_parse_value = NULL;
    cJSON *json_data_parse_time_value = NULL;
    cJSON *json_data_parse_channel_channel_write_key = NULL;
    cJSON *json_data_parse_channel_channel_id_value = NULL;
    cJSON *json_data_parse_channel_metadata = NULL;
    cJSON *json_data_parse_channel_value = NULL;
    cJSON *json_data_parse_channel_user_id = NULL;
    //char *json_print;

    // printf("start_parse_active_http_json\r\n");

    if (http_json_data[0] != '{')
    {
        printf("http_json_data Json Formatting error\n");

        return 0;
    }

    json_data_parse = cJSON_Parse(http_json_data);
    if (json_data_parse == NULL)
    {
        printf("Json Formatting error3\n");

        cJSON_Delete(json_data_parse);
        return 0;
    }
    else
    {
        json_data_parse_value = cJSON_GetObjectItem(json_data_parse, "result");
        if (!(strcmp(json_data_parse_value->valuestring, "success")))
        {
            printf("active:success\r\n");

            json_data_parse_time_value = cJSON_GetObjectItem(json_data_parse, "server_time");
            Server_Timer_GET(json_data_parse_time_value->valuestring);
        }
        else
        {
            printf("active:error\r\n");
            cJSON_Delete(json_data_parse);
            return 0;
        }

        if (cJSON_GetObjectItem(json_data_parse, "channel") != NULL)
        {
            json_data_parse_channel_value = cJSON_GetObjectItem(json_data_parse, "channel");

            //printf("%s\r\n", cJSON_Print(json_data_parse_channel_value));

            json_data_parse_channel_channel_write_key = cJSON_GetObjectItem(json_data_parse_channel_value, "write_key");
            json_data_parse_channel_channel_id_value = cJSON_GetObjectItem(json_data_parse_channel_value, "channel_id");
            json_data_parse_channel_metadata = cJSON_GetObjectItem(json_data_parse_channel_value, "metadata");
            json_data_parse_channel_user_id = cJSON_GetObjectItem(json_data_parse_channel_value, "user_id");

            // printf("metadata: %s\n", json_data_parse_channel_metadata->valuestring);
            Parse_metadata(json_data_parse_channel_metadata->valuestring);
            //printf("api key=%s\r\n", json_data_parse_channel_channel_write_key->valuestring);
            //printf("channel_id=%s\r\n", json_data_parse_channel_channel_id_value->valuestring);

            //写入API-KEY
            sprintf(ApiKey, "%s%c", json_data_parse_channel_channel_write_key->valuestring, '\0');
            //写入channelid
            sprintf(ChannelId, "%s%c", json_data_parse_channel_channel_id_value->valuestring, '\0');
            AT24CXX_Write(CHANNEL_ID_ADD, (uint8_t *)ChannelId, CHANNEL_ID_LEN);
            //写入user_id
            sprintf(USER_ID, "%s%c", json_data_parse_channel_user_id->valuestring, '\0');
            AT24CXX_Write(USER_ID_ADD, (uint8_t *)USER_ID, USER_ID_LEN);
        }
    }
    cJSON_Delete(json_data_parse);
    return 1;
}

//解析http-post返回数据
esp_err_t parse_objects_http_respond(char *http_json_data)
{
    cJSON *json_data_parse = NULL;
    cJSON *json_data_parse_value = NULL;
    cJSON *json_data_parse_errorcode = NULL;

    if (http_json_data[0] != '{')
    {
        printf("http_respond_json_data Json Formatting error\n");
        return 0;
    }

    json_data_parse = cJSON_Parse(http_json_data);
    if (json_data_parse == NULL)
    {

        printf("Json Formatting error3\n");
        cJSON_Delete(json_data_parse);
        return 0;
    }
    else
    {
        json_data_parse_value = cJSON_GetObjectItem(json_data_parse, "result");
        // printf("result: %s\n", json_data_parse_value->valuestring);
        if (!(strcmp(json_data_parse_value->valuestring, "error")))
        {
            json_data_parse_errorcode = cJSON_GetObjectItem(json_data_parse, "errorCode");
            printf("post send error_code=%s\n", json_data_parse_errorcode->valuestring);
            if (!(strcmp(json_data_parse_errorcode->valuestring, "invalid_channel_id"))) //设备空间ID被删除或API——KEY错误，需要重新激活
            {
                // //清空API-KEY存储，激活后获取
                // uint8_t data_write2[33] = "\0";
                // E2prom_Write(0x00, data_write2, 32);

                // //清空channelid，激活后获取
                // uint8_t data_write3[16] = "\0";

                // E2prom_Write(0x20, data_write3, 16);

                fflush(stdout); //使stdout清空，就会立刻输出所有在缓冲区的内容。
                esp_restart();  //芯片复位 函数位于esp_system.h
            }
        }

        json_data_parse_value = cJSON_GetObjectItem(json_data_parse, "metadata");
        if (json_data_parse_value != NULL)
        {
            // printf("metadata: %s\n", json_data_parse_value->valuestring);
            Parse_metadata(json_data_parse_value->valuestring);
        }
    }
    cJSON_Delete(json_data_parse);
    return 1;
}

esp_err_t parse_objects_heart(char *json_data)
{
    cJSON *json_data_parse = NULL;
    cJSON *json_data_parse_value = NULL;
    json_data_parse = cJSON_Parse(json_data);

    if (json_data[0] != '{')
    {
        printf("heart Json Formatting error\n");

        return 0;
    }

    if (json_data_parse == NULL) //如果数据包不为JSON则退出
    {

        printf("Json Formatting error4\n");

        cJSON_Delete(json_data_parse);
        return 0;
    }
    else
    {
        json_data_parse_value = cJSON_GetObjectItem(json_data_parse, "result");
        if (!(strcmp(json_data_parse_value->valuestring, "success")))
        {
            json_data_parse_value = cJSON_GetObjectItem(json_data_parse, "server_time");
            Server_Timer_GET(json_data_parse_value->valuestring);
        }
        else
        {
            printf("active:error\r\n");
            cJSON_Delete(json_data_parse);
            return 0;
        }
    }
    cJSON_Delete(json_data_parse);
    return 1;
}

//解析MQTT      指令
esp_err_t parse_objects_mqtt(char *mqtt_json_data)
{
    cJSON *json_data_parse = NULL;
    cJSON *json_data_string_parse = NULL;
    cJSON *json_data_command_id_parse = NULL;

    //OTA相关
    cJSON *json_data_action = NULL;
    cJSON *json_data_url = NULL;
    cJSON *json_data_vesion = NULL;
    cJSON *json_data_set_state = NULL;

    json_data_parse = cJSON_Parse(mqtt_json_data);
    // printf("%s", cJSON_Print(json_data_parse));

    if (mqtt_json_data[0] != '{')
    {
        printf("mqtt_json_data Json Formatting error\n");

        return 0;
    }

    if (json_data_parse == NULL) //如果数据包不为JSON则退出
    {

        printf("Json Formatting error5\n");

        cJSON_Delete(json_data_parse);
        return 0;
    }

    json_data_string_parse = cJSON_GetObjectItem(json_data_parse, "command_string");
    if (json_data_string_parse != NULL)
    {
        json_data_command_id_parse = cJSON_GetObjectItem(json_data_parse, "command_id");
        strncpy(mqtt_json_s.mqtt_command_id, json_data_command_id_parse->valuestring, strlen(json_data_command_id_parse->valuestring));
        strncpy(mqtt_json_s.mqtt_string, json_data_string_parse->valuestring, strlen(json_data_string_parse->valuestring));

        post_status = POST_NORMAL;
        if (Binary_dp != NULL)
        {
            // xSemaphoreGive(Binary_Http_Send);
        }
        // need_send = 1;
        json_data_string_parse = cJSON_Parse(json_data_string_parse->valuestring); //将command_string再次构建成json格式，以便二次解析
        if (json_data_string_parse != NULL)
        {
            // printf("MQTT-command_string  = %s\r\n", cJSON_Print(json_data_string_parse));

            if ((json_data_action = cJSON_GetObjectItem(json_data_string_parse, "action")) != NULL)
            {
                //如果命令是OTA
                if (strcmp(json_data_action->valuestring, "ota") == 0)
                {
                    printf("OTA命令进入\r\n");
                    if ((json_data_vesion = cJSON_GetObjectItem(json_data_string_parse, "version")) != NULL &&
                        (json_data_url = cJSON_GetObjectItem(json_data_string_parse, "url")) != NULL)
                    {
                        if (strcmp(json_data_vesion->valuestring, FIRMWARE) != 0) //与当前 版本号 对比
                        {
                            strcpy(mqtt_json_s.mqtt_ota_url, json_data_url->valuestring);
                            // E2prom_page_Write(ota_url_add, (uint8_t *)mqtt_json_s.mqtt_ota_url, 128);
                            printf("OTA_URL=%s\r\n OTA_VERSION=%s\r\n", mqtt_json_s.mqtt_ota_url, json_data_vesion->valuestring);
                            ota_start(); //启动OTA
                        }
                        else
                        {
                            printf("当前版本无需升级 \r\n");
                        }
                    }
                }

                else if (strcmp(json_data_action->valuestring, "command") == 0)
                {
                    if ((json_data_set_state = cJSON_GetObjectItem(json_data_string_parse, "set_state")) != NULL)
                    {
                        Switch_Relay(json_data_set_state->valueint);
                    }
                    else if ((json_data_set_state = cJSON_GetObjectItem(json_data_string_parse, "set_state_plug1")) != NULL)
                    {
                        Switch_Relay(json_data_set_state->valueint);
                    }
                }
                else
                {
                    printf("Action 非许可 \r\n");
                }
            }
            else if ((json_data_action = cJSON_GetObjectItem(json_data_string_parse, "command")) != NULL)
            {
                printf("command CMD!\r\n");
                ParseTcpUartCmd(cJSON_Print(json_data_string_parse));
            }
            else
            {
                printf("非许可命令\r\n");
            }
        }
    }
    else
    {
        printf("Json Formatting error6\n");
        cJSON_Delete(json_data_parse);
        cJSON_Delete(json_data_string_parse);
        return 0;
    }
    cJSON_Delete(json_data_parse);
    cJSON_Delete(json_data_string_parse);

    return 1;
}

uint8_t Create_NET_Json(char *status_buff)
{
    char *OutBuffer;
    uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;
    wifi_ap_record_t wifidata;
    uint8_t mac_sys[6] = {0};
    char mac_buff[32] = {0};
    char ssid64_buff[64] = {0};

    pJsonRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());

    if (esp_wifi_sta_get_ap_info(&wifidata) == 0)
    {
        itoa(wifidata.rssi, mqtt_json_s.mqtt_Rssi, 10);
    }
    cJSON_AddItemToObject(pJsonRoot, "field6", cJSON_CreateString(mqtt_json_s.mqtt_Rssi));
    OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
    cJSON_Delete(pJsonRoot);                       //delete cjson root
    len = strlen(OutBuffer);
    printf("len:%d\n%s\n", len, OutBuffer);
    // SaveBuffer = (uint8_t *)malloc(len);
    SaveBuffer = (uint8_t *)malloc(len);
    memcpy(SaveBuffer, OutBuffer, len);
    xSemaphoreTake(Cache_muxtex, -1);
    DataSave(SaveBuffer, len);
    xSemaphoreGive(Cache_muxtex);
    free(OutBuffer);
    free(SaveBuffer);

    esp_read_mac(mac_sys, 0); //获取芯片内部默认出厂MAC，
    sprintf(mac_buff,
            "mac=%02x:%02x:%02x:%02x:%02x:%02x",
            mac_sys[0],
            mac_sys[1],
            mac_sys[2],
            mac_sys[3],
            mac_sys[4],
            mac_sys[5]);
    base64_encode(wifi_data.wifi_ssid, strlen(wifi_data.wifi_ssid), ssid64_buff, sizeof(ssid64_buff));

    sprintf(status_buff, "],\"status\":\"%s\",\"ssid_base64\":\"%s\"}", mac_buff, ssid64_buff);
    return strlen(status_buff);
}

void create_http_json(creat_json *pCreat_json, uint8_t flag)
{
    // printf("INTO CREATE_HTTP_JSON\r\n");
    //creat_json *pCreat_json = malloc(sizeof(creat_json));
    cJSON *root = cJSON_CreateObject();
    // cJSON *item = cJSON_CreateObject();
    cJSON *next = cJSON_CreateObject();
    cJSON *fe_body = cJSON_CreateArray();
    uint8_t mac_sys[6] = {0};
    char mac_buff[32] = {0};
    char ssid64_buff[64] = {0};

    wifi_ap_record_t wifidata;

    cJSON_AddItemToObject(root, "feeds", fe_body);
    cJSON_AddItemToArray(fe_body, next);
    // cJSON_AddItemToObject(next, "created_at", cJSON_CreateString(http_json_c.http_time));
    cJSON_AddStringToObject(next, "created_at", (const char *)Server_Timer_SEND());
    cJSON_AddItemToObject(next, "field1", cJSON_CreateNumber(mqtt_json_s.mqtt_switch_status));
    if (mqtt_json_s.mqtt_switch_status == 1 && CSE_Status == true)
    {
        cJSON_AddItemToObject(next, "field2", cJSON_CreateNumber(mqtt_json_s.mqtt_Voltage));
        cJSON_AddItemToObject(next, "field3", cJSON_CreateNumber(mqtt_json_s.mqtt_Current));
        cJSON_AddItemToObject(next, "field4", cJSON_CreateNumber(mqtt_json_s.mqtt_Power));
        if (CSE_Energy_Status == true)
        {
            CSE_Energy_Status = false;
            cJSON_AddItemToObject(next, "field5", cJSON_CreateNumber(mqtt_json_s.mqtt_Energy));
        }
    }

    if (RS485_status == true)
    {
        RS485_status = false;
        cJSON_AddItemToObject(next, "field8", cJSON_CreateNumber(ext_tem));
        cJSON_AddItemToObject(next, "field9", cJSON_CreateNumber(ext_hum));
    }
    if (DS18b20_status == true)
    {
        DS18b20_status = false;
        cJSON_AddItemToObject(next, "field7", cJSON_CreateString(mqtt_json_s.mqtt_DS18B20_TEM)); //18B20温度
    }

    esp_read_mac(mac_sys, 0); //获取芯片内部默认出厂MAC，
    sprintf(mac_buff,
            "mac=%02x:%02x:%02x:%02x:%02x:%02x",
            mac_sys[0],
            mac_sys[1],
            mac_sys[2],
            mac_sys[3],
            mac_sys[4],
            mac_sys[5]);
    base64_encode(wifi_data.wifi_ssid, strlen(wifi_data.wifi_ssid), ssid64_buff, sizeof(ssid64_buff));

    if (esp_wifi_sta_get_ap_info(&wifidata) == 0)
    {
        cJSON_AddItemToObject(next, "field6", cJSON_CreateNumber(wifidata.rssi)); //WIFI RSSI
    }
    cJSON_AddItemToObject(root, "status", cJSON_CreateString(mac_buff));
    cJSON_AddItemToObject(root, "ssid_base64", cJSON_CreateString(ssid64_buff));

    char *cjson_printunformat;
    // cjson_printunformat = cJSON_PrintUnformatted(root); //将整个 json 转换成字符串 ，没有格式
    cjson_printunformat = cJSON_PrintUnformatted(root); //将整个 json 转换成字符串 ，有格式
    //printf("status_creat_json= %s\r\n", cjson_printunformat);

    pCreat_json->len = strlen(cjson_printunformat); //  creat_json_c 是整个json 所占的长度
    //pCreat_json->creat_json_b=cjson_printunformat;
    //pCreat_json->creat_json_b=malloc(pCreat_json->creat_json_c);
    bzero(pCreat_json->buff, sizeof(pCreat_json->len)); //  creat_json_b 是整个json 包
    memcpy(pCreat_json->buff, cjson_printunformat, pCreat_json->len);
    //printf("http_json=%s\n",pCreat_json->creat_json_b);
    free(cjson_printunformat);
    cJSON_Delete(root);
    //return pCreat_json;
}

esp_err_t ParseTcpUartCmd(char *pcCmdBuffer)
{
    char send_buf[128] = {0};
    sprintf(send_buf, "{\"status\":0,\"code\": 0}");
    if (NULL == pcCmdBuffer) //null
    {
        return ESP_FAIL;
    }

    cJSON *pJson = cJSON_Parse(pcCmdBuffer); //parse json data
    if (NULL == pJson)
    {
        cJSON_Delete(pJson); //delete pJson

        return ESP_FAIL;
    }

    cJSON *pSub = cJSON_GetObjectItem(pJson, "Command"); //"Command"
    if (NULL != pSub)
    {
        if (!strcmp((char const *)pSub->valuestring, "SetupProduct")) //Command:SetupProduct
        {
            pSub = cJSON_GetObjectItem(pJson, "Password"); //"Password"
            if (NULL != pSub)
            {
                if (!strcmp((char const *)pSub->valuestring, "CloudForce"))
                {
                    E2prom_empty_all();
                    pSub = cJSON_GetObjectItem(pJson, "ProductID"); //"ProductID"
                    if (NULL != pSub)
                    {
                        printf("ProductID= %s, len= %d\n", pSub->valuestring, strlen(pSub->valuestring));
                        AT24CXX_Write(PRODUCT_ID_ADDR, (uint8_t *)pSub->valuestring, PRODUCT_ID_LEN); //save ProductID
                    }

                    pSub = cJSON_GetObjectItem(pJson, "SeriesNumber"); //"SeriesNumber"
                    if (NULL != pSub)
                    {
                        printf("SeriesNumber= %s, len=%d\n", pSub->valuestring, strlen(pSub->valuestring));
                        AT24CXX_Write(SERISE_NUM_ADDR, (uint8_t *)pSub->valuestring, SERISE_NUM_LEN); //save SeriesNumber
                    }

                    pSub = cJSON_GetObjectItem(pJson, "Host"); //"Host"
                    if (NULL != pSub)
                    {
                        printf("Host= %s, len=%d\n", pSub->valuestring, strlen(pSub->valuestring));
                        AT24CXX_Write(WEB_HOST_ADD, (uint8_t *)pSub->valuestring, WEB_HOST_LEN); //save SeriesNumber
                    }

                    pSub = cJSON_GetObjectItem(pJson, "apn"); //"apn"
                    if (NULL != pSub)
                    {

                        //E2prom_Write(APN_ADDR, (uint8_t *)pSub->valuestring, strlen(pSub->valuestring), 1); //save apn
                    }

                    pSub = cJSON_GetObjectItem(pJson, "user"); //"user"
                    if (NULL != pSub)
                    {

                        //E2prom_Write(BEARER_USER_ADDR, (uint8_t *)pSub->valuestring, strlen(pSub->valuestring), 1); //save user
                    }

                    pSub = cJSON_GetObjectItem(pJson, "pwd"); //"pwd"
                    if (NULL != pSub)
                    {

                        // E2prom_Write(BEARER_PWD_ADDR, (uint8_t *)pSub->valuestring, strlen(pSub->valuestring), 1); //save pwd
                    }

                    printf("SetupProduct Successed !");
                    printf("{\"status\":0,\"code\": 0}");

                    if (start_AP == 1)
                    {
                        printf("%s\n", send_buf);
                        tcp_send_buff(send_buf, sizeof(send_buf));
                    }
                    vTaskDelay(3000 / portTICK_RATE_MS);
                    cJSON_Delete(pJson);
                    fflush(stdout); //使stdout清空，就会立刻输出所有在缓冲区的内容。
                    esp_restart();  //芯片复位 函数位于esp_system.h

                    return ESP_OK;
                }
            }
        }

        else if (!strcmp((char const *)pSub->valuestring, "SetupWifi")) //Command:SetupWifi
        {
            pSub = cJSON_GetObjectItem(pJson, "SSID"); //"SSID"
            if (NULL != pSub)
            {
                bzero(wifi_data.wifi_ssid, sizeof(wifi_data.wifi_ssid));
                strcpy(wifi_data.wifi_ssid, pSub->valuestring);
                AT24CXX_Write(WIFI_SSID_ADD, (uint8_t *)wifi_data.wifi_ssid, sizeof(wifi_data.wifi_ssid));
                printf("WIFI_SSID = %s\r\n", pSub->valuestring);
            }

            pSub = cJSON_GetObjectItem(pJson, "password"); //"password"
            if (NULL != pSub)
            {
                bzero(wifi_data.wifi_pwd, sizeof(wifi_data.wifi_pwd));
                strcpy(wifi_data.wifi_pwd, pSub->valuestring);
                AT24CXX_Write(WIFI_PASSWORD_ADD, (uint8_t *)wifi_data.wifi_pwd, sizeof(wifi_data.wifi_pwd));
                printf("WIFI_PWD = %s\r\n", pSub->valuestring);
            }
            AT24CXX_WriteOneByte(net_mode_add, NET_WIFI); //写入net_mode
            net_mode = NET_WIFI;
            start_user_wifi();

            cJSON_Delete(pJson); //delete pJson

            return 1;
        }
        else if (!strcmp((char const *)pSub->valuestring, "ReadProduct")) //Command:ReadProduct
        {
            char mac_buff[18];
            char *json_temp;
            uint8_t mac_sys[6] = {0};
            cJSON *root = cJSON_CreateObject();

            AT24CXX_Read(SERISE_NUM_ADDR, (uint8_t *)SerialNum, SERISE_NUM_LEN);
            AT24CXX_Read(PRODUCT_ID_ADDR, (uint8_t *)ProductId, PRODUCT_ID_LEN);
            AT24CXX_Read(WEB_HOST_ADD, (uint8_t *)WEB_SERVER, WEB_HOST_LEN);
            AT24CXX_Read(CHANNEL_ID_ADD, (uint8_t *)ChannelId, CHANNEL_ID_LEN);
            AT24CXX_Read(USER_ID_ADD, (uint8_t *)USER_ID, USER_ID_LEN);

            esp_read_mac(mac_sys, 0); //获取芯片内部默认出厂MAC，
            sprintf(mac_buff,
                    "%02x:%02x:%02x:%02x:%02x:%02x",
                    mac_sys[0],
                    mac_sys[1],
                    mac_sys[2],
                    mac_sys[3],
                    mac_sys[4],
                    mac_sys[5]);

            cJSON_AddItemToObject(root, "ProductID", cJSON_CreateString(ProductId));
            cJSON_AddItemToObject(root, "SeriesNumber", cJSON_CreateString(SerialNum));
            cJSON_AddItemToObject(root, "Host", cJSON_CreateString(WEB_SERVER));
            cJSON_AddItemToObject(root, "CHANNEL_ID", cJSON_CreateString(ChannelId));
            cJSON_AddItemToObject(root, "USER_ID", cJSON_CreateString(USER_ID));
            cJSON_AddItemToObject(root, "MAC", cJSON_CreateString(mac_buff));
            cJSON_AddItemToObject(root, "firmware", cJSON_CreateString(FIRMWARE));

            // sprintf(json_temp, "%s", cJSON_PrintUnformatted(root));
            json_temp = cJSON_PrintUnformatted(root);
            printf("%s\n", json_temp);
            cJSON_Delete(root); //delete pJson
            free(json_temp);
        }
        else if (!strcmp((char const *)pSub->valuestring, "CheckSensors")) //Command:ReadProduct
        {
            // if (human_chack == 1)
            // {
            //     printf("{\"result\":\"OK\",\"human\":\"OK\"}\n");
            // }
            // else
            // {
            //     printf("{\"result\":\"ERROR\",\"human\":\"ERROR\"}\n");
            // }
        }
    }

    cJSON_Delete(pJson); //delete pJson

    return ESP_FAIL;
}

//读取EEPROM中的metadata

void Read_Metadate(void)
{

    fn_dp = AT24CXX_ReadLenByte(fn_dp_add, 4);             //数据发送频率
    fn_485_th = AT24CXX_ReadLenByte(fn_485_th_add, 4);     //485温湿度探头
    fn_485_sth = AT24CXX_ReadLenByte(fn_485_sth_add, 4);   //485 土壤探头
    fn_ext = AT24CXX_ReadLenByte(fn_ext_add, 4);           //18b20
    fn_energy = AT24CXX_ReadLenByte(fn_energy_add, 4);     //电能信息：电压/电流/功率
    fn_ele_quan = AT24CXX_ReadLenByte(fn_ele_quan_add, 4); //用电量统计
    cg_data_led = AT24CXX_ReadOneByte(cg_data_led_add);    //发送数据 LED状态 0关闭，1打开
    net_mode = AT24CXX_ReadOneByte(net_mode_add);          //上网模式选择 0：自动模式 1：lan模式 2：wifi模式

    printf("metadata:\nfn_dp=%d\nfn_485_th=%d\nfn_485_sth=%d\nfn_ext=%d\nfn_energy=%d\nfn_ele_quan=%d\ncg_data_led=%d\nnet_mode=%d\n",
           fn_dp, fn_485_th, fn_485_sth, fn_ext, fn_energy, fn_ele_quan, cg_data_led, net_mode);
}
