#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include "esp_system.h"
#include "Json_parse.h"
#include "Nvs.h"
#include "ServerTimer.h"
#include "Http.h"
#include "driver/gpio.h"

#include "esp_wifi.h"
#include "Smartconfig.h"
#include "E2prom.h"
#include "Http.h"
#include "RS485_Read.h"
#include "Bluetooth.h"
#include "Switch.h"
#include "Led.h"
#include "tcp_bsp.h"
#include "ota.h"
#include "RS485_Read.h"
#include "ds18b20.h"
#include "RtcUsr.h"

//metadata 参数
unsigned long fn_dp = 60; //数据发送频率 默认60s
unsigned long fn_th = 0;  //温湿度频率
uint8_t cg_data_led = 1;  //发送数据 LED状态 0：不闪烁 1：闪烁
uint8_t net_mode = 0;     //上网模式选择 0：自动模式 1：lan模式 2：wifi模式

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

//解析metadata
static short Parse_metadata(char *ptrptr)
{
    // bool fn_flag = 0;
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

    cJSON *pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_th"); //"fn_th"
    if (NULL != pSubSubSub)
    {

        if ((unsigned long)pSubSubSub->valueint != fn_th)
        {
            fn_th = (unsigned long)pSubSubSub->valueint;
            printf("fn_th = %ld\n", fn_th);
        }
    }

    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_dp"); //"fn_dp"
    if (NULL != pSubSubSub)
    {
        if ((unsigned long)pSubSubSub->valueint != fn_dp)
        {
            // fn_flag = 1;
            fn_dp = (unsigned long)pSubSubSub->valueint;
            printf("fn_dp = %ld\n", fn_dp);
        }
    }

    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "cg_data_led"); //"cg_data_led"
    if (NULL != pSubSubSub)
    {

        if ((uint8_t)pSubSubSub->valueint != cg_data_led)
        {
            cg_data_led = (uint8_t)pSubSubSub->valueint;
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
            printf("net_mode = %d\n", net_mode);
        }
    }

    cJSON_Delete(pJsonJson);
    return 1;
}

int read_bluetooth(void)
{
    uint8_t bluetooth_sta[256];

    E2prom_BluRead(0x00, bluetooth_sta, 256);
    printf("bluetooth_sta=%s\n", bluetooth_sta);
    if (strlen((char *)bluetooth_sta) == 0) //未读到蓝牙配置信息
    {
        return 0;
    }
    uint8_t ret = parse_objects_bluetooth((char *)bluetooth_sta);
    if ((ret == BLU_PWD_REFUSE) || (ret == BLU_JSON_FORMAT_ERROR))
    {
        return 0;
    }
    return 1;
}

esp_err_t parse_objects_bluetooth(char *blu_json_data)
{
    cJSON *cjson_blu_data_parse = NULL;
    cJSON *cjson_blu_data_parse_command = NULL;
    // cJSON *cjson_blu_data_parse_wifissid = NULL;
    // cJSON *cjson_blu_data_parse_wifipwd = NULL;
    // cJSON *cjson_blu_data_parse_ob = NULL;
    //cJSON *cjson_blu_data_parse_devicepwd = NULL;

    int ble_return = 0;

    printf("start_ble_parse_json\r\n");
    if (blu_json_data[0] != '{')
    {
        printf("blu_json_data Json Formatting error\n");
        return 0;
    }

    //数据包包含NULL则直接返回error
    if (strstr(blu_json_data, "null") != NULL) //需要解析的字符串内含有null
    {
        printf("there is null in blu data\r\n");
        return BLU_PWD_REFUSE;
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

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, 8000 / portTICK_RATE_MS);
    if (wifi_connect_sta == connect_Y)
    {
        ble_return = BLU_RESULT_SUCCESS;
        need_reactivate = 1;
        http_activate();
    }
    else if (wifi_connect_sta == connect_N)
    {
        ble_return = BLU_WIFI_ERR;
    }

    cJSON_Delete(cjson_blu_data_parse);
    return ble_return;
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
            // Server_Timer_GET(json_data_parse_time_value->valuestring);
            Check_Update_UTCtime(json_data_parse_time_value->valuestring);
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

            // printf("metadata: %s\n", json_data_parse_channel_metadata->valuestring);
            Parse_metadata(json_data_parse_channel_metadata->valuestring);
            //printf("api key=%s\r\n", json_data_parse_channel_channel_write_key->valuestring);
            //printf("channel_id=%s\r\n", json_data_parse_channel_channel_id_value->valuestring);

            //写入API-KEY
            sprintf(ApiKey, "%s%c", json_data_parse_channel_channel_write_key->valuestring, '\0');
            //printf("api key=%s\r\n", ApiKey);
            E2prom_Write(0x00, (uint8_t *)ApiKey, 32);

            //写入channelid
            sprintf(ChannelId, "%s%c", json_data_parse_channel_channel_id_value->valuestring, '\0');
            //printf("channel_id=%s\r\n", ChannelId);
            E2prom_Write(0x20, (uint8_t *)ChannelId, strlen(ChannelId));
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
                //清空API-KEY存储，激活后获取
                uint8_t data_write2[33] = "\0";
                E2prom_Write(0x00, data_write2, 32);

                //清空channelid，激活后获取
                uint8_t data_write3[16] = "\0";

                E2prom_Write(0x20, data_write3, 16);

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
    char *json_print;
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
        json_data_parse_value = cJSON_GetObjectItem(json_data_parse, "server_time");
        // Server_Timer_GET(json_data_parse_value->valuestring);
        Check_Update_UTCtime(json_data_parse_value->valuestring);
        json_print = cJSON_Print(json_data_parse_value);
        printf("json_data_parse_value %s\n", json_print);
    }
    free(json_print);
    cJSON_Delete(json_data_parse);

    return 1;
}

/*

json_data_parse_value "2019-02-12T07:40:16Z"
{
        "command_id":   "1631661",
        "command_string":       "{\"action\":\"command\",\"set_state_plug1\":1,\"channel_id\":\"4200\",\"s_port\":\"port1\",\"c_type\":\"loop\",\"s_id\":\"42\"}",
        "position":     1,
        "created_at":   "2019-02-12T07:40:16Z",
        "executed_at":  null,
        "channel_id":   "4200",
        "user_id":      "9121EFDE-0A2F-4846-8F71-C10098FB9A23"
}


{
	"action": "command",
	"set_state_plug1": 1,
	"channel_id": "4200",
	"s_port": "port1",
	"c_type": "loop",
	"s_id": "42"
}

*/
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

        xSemaphoreGive(Binary_Http_Send);
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
                        Mqtt_Switch_Relay(json_data_set_state->valueint);
                    }
                    else if ((json_data_set_state = cJSON_GetObjectItem(json_data_string_parse, "set_state_plug1")) != NULL)
                    {
                        Mqtt_Switch_Relay(json_data_set_state->valueint);
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

void create_http_json(creat_json *pCreat_json)
{
    printf("INTO CREATE_HTTP_JSON\r\n");
    cJSON *root = cJSON_CreateObject();
    cJSON *next = cJSON_CreateObject();
    cJSON *fe_body = cJSON_CreateArray();

    // strncpy(http_json_c.http_time, Server_Timer_SEND(), 24);
    Read_UTCtime(http_json_c.http_time, sizeof(http_json_c.http_time));
    wifi_ap_record_t wifidata;
    esp_wifi_sta_get_ap_info(&wifidata);

    snprintf(mqtt_json_s.mqtt_etx_tem, sizeof(mqtt_json_s.mqtt_etx_tem) - 1, "%4.2f", ext_tem);
    snprintf(mqtt_json_s.mqtt_etx_hum, sizeof(mqtt_json_s.mqtt_etx_tem) - 1, "%4.2f", ext_hum);
    snprintf(mqtt_json_s.mqtt_DS18B20_TEM, sizeof(mqtt_json_s.mqtt_etx_tem) - 1, "%4.2f", DS18B20_TEM);

    cJSON_AddItemToObject(root, "feeds", fe_body);
    cJSON_AddItemToArray(fe_body, next);
    cJSON_AddItemToObject(next, "created_at", cJSON_CreateString(http_json_c.http_time));
    cJSON_AddItemToObject(next, "field1", cJSON_CreateNumber(mqtt_json_s.mqtt_switch_status));
    if (mqtt_json_s.mqtt_switch_status == 1)
    {
        cJSON_AddItemToObject(next, "field2", cJSON_CreateNumber(mqtt_json_s.mqtt_Voltage));
        cJSON_AddItemToObject(next, "field3", cJSON_CreateNumber(mqtt_json_s.mqtt_Current));
        cJSON_AddItemToObject(next, "field4", cJSON_CreateNumber(mqtt_json_s.mqtt_Power));
        cJSON_AddItemToObject(next, "field5", cJSON_CreateNumber(mqtt_json_s.mqtt_Energy));
    }
    cJSON_AddItemToObject(next, "field6", cJSON_CreateNumber(wifidata.rssi));                //WIFI RSSI
    cJSON_AddItemToObject(next, "field8", cJSON_CreateString(mqtt_json_s.mqtt_etx_tem));     //485温度
    cJSON_AddItemToObject(next, "field9", cJSON_CreateString(mqtt_json_s.mqtt_etx_hum));     //485湿度
    cJSON_AddItemToObject(next, "field7", cJSON_CreateString(mqtt_json_s.mqtt_DS18B20_TEM)); //18B20温度

    char *cjson_printunformat;
    cjson_printunformat = cJSON_PrintUnformatted(root);
    //pCreat_json->creat_json_b = cJSON_PrintUnformatted(root);
    //pCreat_json->creat_json_c = strlen(cJSON_PrintUnformatted(root));

    pCreat_json->creat_json_c = strlen(cjson_printunformat);
    //pCreat_json->creat_json_b=cjson_printunformat;
    //pCreat_json->creat_json_b=malloc(pCreat_json->creat_json_c);
    bzero(pCreat_json->creat_json_b, sizeof(pCreat_json->creat_json_b));
    memcpy(pCreat_json->creat_json_b, cjson_printunformat, pCreat_json->creat_json_c);
    printf("http_json=%s\n", pCreat_json->creat_json_b);
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
                    //       E2prom_Write(PRODUCTURI_FLAG_ADDR, PRODUCT_URI, strlen(PRODUCT_URI), 1); //save product-uri flag

                    pSub = cJSON_GetObjectItem(pJson, "ProductID"); //"ProductID"
                    if (NULL != pSub)
                    {
                        printf("ProductID= %s\n", pSub->valuestring);
                        E2prom_Write(PRODUCT_ID_ADDR, (uint8_t *)pSub->valuestring, strlen(pSub->valuestring)); //save ProductID
                    }

                    pSub = cJSON_GetObjectItem(pJson, "SeriesNumber"); //"SeriesNumber"
                    if (NULL != pSub)
                    {
                        printf("SeriesNumber= %s\n", pSub->valuestring);
                        E2prom_Write(SERISE_NUM_ADDR, (uint8_t *)pSub->valuestring, strlen(pSub->valuestring)); //save SeriesNumber
                    }

                    pSub = cJSON_GetObjectItem(pJson, "Host"); //"Host"
                    if (NULL != pSub)
                    {

                        //E2prom_Write(HOST_ADDR, (uint8_t *)pSub->valuestring, strlen(pSub->valuestring), 1); //save host in at24c08
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

                    //清空API-KEY存储，激活后获取
                    uint8_t data_write2[33] = "\0";
                    E2prom_Write(0x00, data_write2, 32);

                    //清空channelid，激活后获取
                    uint8_t data_write3[16] = "\0";
                    E2prom_Write(0x20, data_write3, 16);

                    uint8_t zerobuf[256] = "\0";
                    E2prom_BluWrite(0x00, (uint8_t *)zerobuf, 256); //清空蓝牙
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
                printf("WIFI_SSID = %s\r\n", pSub->valuestring);
            }

            pSub = cJSON_GetObjectItem(pJson, "password"); //"password"
            if (NULL != pSub)
            {
                bzero(wifi_data.wifi_pwd, sizeof(wifi_data.wifi_pwd));
                strcpy(wifi_data.wifi_pwd, pSub->valuestring);
                printf("WIFI_PWD = %s\r\n", pSub->valuestring);
            }

            // initialise_wifi(wifi_data.wifi_ssid, wifi_data.wifi_pwd);
            initialise_wifi();
            cJSON_Delete(pJson); //delete pJson

            return 1;
        }
    }

    cJSON_Delete(pJson); //delete pJson

    return ESP_FAIL;
}
