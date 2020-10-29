#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include "esp_system.h"
#include "esp_log.h"

#include "ServerTimer.h"
#include "Http.h"

#include "esp_wifi.h"
#include "Smartconfig.h"
#include "E2prom.h"
#include "Http.h"
#include "Cache_data.h"
#include "Bluetooth.h"
#include "ota.h"
#include "Led.h"
// #include "tcp_bsp.h"
#include "my_base64.h"
#include "RS485_Read.h"
#include "ds18b20.h"
#include "RtcUsr.h"
#include "CSE7759B.h"
#include "Switch.h"
#include "Mqtt.h"
#include "EC20.h"

#include "Json_parse.h"

#define TAG "Json_parse"

//metadata 相关变量
// uint8_t fn_set_flag = 0;     //metadata 读取标志，未读取则使用下面固定参数
uint32_t fn_dp = 300;     //数据发送频率
uint32_t fn_485_t = 0;    //485 温度探头
uint32_t fn_485_th = 0;   //485温湿度探头
uint32_t fn_485_sth = 0;  //485 土壤探头
uint32_t fn_485_ws = 0;   //485 风速
uint32_t fn_485_lt = 0;   //485 光照
uint32_t fn_485_co2 = 0;  //485二氧化碳
uint32_t fn_ext = 0;      //18b20
uint32_t fn_sw_e = 60;    //电能信息：电压/电流/功率
uint32_t fn_sw_pc = 3600; //用电量统计
uint8_t cg_data_led = 1;  //发送数据 LED状态 0关闭，1打开
uint8_t net_mode = 0;     //上网模式选择 0：自动模式 1：lan模式 2：wifi模式
uint8_t de_sw_s = 0;      //开关默认上电状态
uint32_t fn_sw_on = 3600; //开启时长统计

// field num 相关参数
uint8_t sw_s_f_num = 1;      //开关状态
uint8_t sw_v_f_num = 2;      //插座电压
uint8_t sw_c_f_num = 3;      //插座电流
uint8_t sw_p_f_num = 4;      //插座功率
uint8_t sw_pc_f_num = 5;     //累计用电量
uint8_t rssi_w_f_num = 6;    //wifi信号
uint8_t rssi_g_f_num = 7;    //4G信号
uint8_t sw_on_f_num = 8;     //累积开启时长
uint8_t r1_light_f_num = 9;  //485光照
uint8_t r1_th_t_f_num = 10;  //485空气温度
uint8_t r1_th_h_f_num = 11;  //485空气湿度
uint8_t r1_sth_t_f_num = 12; //485土壤温度
uint8_t r1_sth_h_f_num = 13; //485土壤湿度
uint8_t e1_t_f_num = 14;     //DS18B20温度
uint8_t r1_t_f_num = 15;     //485温度探头温度
uint8_t r1_ws_f_num = 16;    //485风速
uint8_t r1_co2_f_num = 17;   //485 CO2
uint8_t r1_ph_f_num = 18;    //485 PH
uint8_t r1_co2_t_f_num = 19; // CO2 温度
uint8_t r1_co2_h_f_num = 20; //CO2 湿度

//
char SerialNum[SERISE_NUM_LEN] = {0};
char ProductId[PRODUCT_ID_LEN] = {0};
char ApiKey[API_KEY_LEN] = {0};
char ChannelId[CHANNEL_ID_LEN] = {0};
char USER_ID[USER_ID_LEN] = {0};
char WEB_SERVER[WEB_HOST_LEN] = {0};
char WEB_PORT[5] = {0};
char MQTT_SERVER[WEB_HOST_LEN] = {0};
char MQTT_PORT[5] = {0};
char BleName[100] = {0};
char SIM_APN[32] = {0};
char SIM_USER[32] = {0};
char SIM_PWD[32] = {0};

//cali 相关 f1_a,f1_b,f1_a,f2_b,,,,,
f_cali f_cali_u[40] = {
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
    {1},
    {0},
};

const char f_cali_str[40][6] =
    {
        "f1_a", //
        "f1_b", //
        "f2_a",
        "f2_b",
        "f3_a",
        "f3_b",
        "f4_a",
        "f4_b",
        "f5_a",
        "f5_b",
        "f6_a",
        "f6_b",
        "f7_a",
        "f7_b",
        "f8_a",
        "f8_b",
        "f9_a",
        "f9_b",
        "f10_a",
        "f10_b",
        "f11_a",
        "f11_b",
        "f12_a",
        "f12_b",
        "f13_a",
        "f13_b",
        "f14_a",
        "f14_b",
        "f15_a",
        "f15_b",
        "f16_a",
        "f16_b",
        "f17_a",
        "f17_b",
        "f18_a",
        "f18_b",
        "f19_a",
        "f19_b",
        "f20_a",
        "f20_b"};

static short Parse_fields_num(char *ptrptr);
static short Parse_cali_dat(char *ptrptr);

void Create_fields_num(char *read_buf);

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
            E2P_WriteLenByte(FN_DP_ADD, fn_dp, 4);
            printf("fn_dp = %d\n", fn_dp);
        }
    }

    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_485_t"); //
    if (NULL != pSubSubSub)
    {

        if ((uint32_t)pSubSubSub->valueint != fn_485_t)
        {
            fn_485_t = (uint32_t)pSubSubSub->valueint;
            E2P_WriteLenByte(FN_485_T_ADD, fn_485_t, 4);
            printf("fn_485_th = %d\n", fn_485_t);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_485_th"); //""
    if (NULL != pSubSubSub)
    {

        if ((uint32_t)pSubSubSub->valueint != fn_485_th)
        {
            fn_485_th = (uint32_t)pSubSubSub->valueint;
            E2P_WriteLenByte(FN_485_TH_ADD, fn_485_th, 4);
            printf("fn_485_th = %d\n", fn_485_th);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_485_sth"); //"fn_485_sth"
    if (NULL != pSubSubSub)
    {
        if ((uint32_t)pSubSubSub->valueint != fn_485_sth)
        {
            fn_485_sth = (uint32_t)pSubSubSub->valueint;
            E2P_WriteLenByte(FN_485_STH_ADD, fn_485_sth, 4);
            printf("fn_485_sth = %d\n", fn_485_sth);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_485_ws"); //
    if (NULL != pSubSubSub)
    {
        if ((uint32_t)pSubSubSub->valueint != fn_485_ws)
        {
            fn_485_ws = (uint32_t)pSubSubSub->valueint;
            E2P_WriteLenByte(FN_485_WS_ADD, fn_485_ws, 4);
            printf("fn_485_ws = %d\n", fn_485_ws);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_485_lt"); //
    if (NULL != pSubSubSub)
    {
        if ((uint32_t)pSubSubSub->valueint != fn_485_lt)
        {
            fn_485_lt = (uint32_t)pSubSubSub->valueint;
            E2P_WriteLenByte(FN_485_LT_ADD, fn_485_lt, 4);
            printf("fn_485_lt = %d\n", fn_485_lt);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_485_co2"); //
    if (NULL != pSubSubSub)
    {
        if ((uint32_t)pSubSubSub->valueint != fn_485_co2)
        {
            fn_485_co2 = (uint32_t)pSubSubSub->valueint;
            E2P_WriteLenByte(FN_485_CO2_ADD, fn_485_co2, 4);
            printf("fn_485_co2 = %d\n", fn_485_co2);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_ext"); //"fn_ext"
    if (NULL != pSubSubSub)
    {
        if ((uint32_t)pSubSubSub->valueint != fn_ext)
        {
            fn_ext = (uint32_t)pSubSubSub->valueint;
            E2P_WriteLenByte(FN_EXT_ADD, fn_ext, 4);
            printf("fn_ext = %d\n", fn_ext);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_sw_e"); //"fn_sw_e"
    if (NULL != pSubSubSub)
    {
        if ((uint32_t)pSubSubSub->valueint != fn_sw_e)
        {
            fn_sw_e = (uint32_t)pSubSubSub->valueint;
            E2P_WriteLenByte(FN_ENERGY_ADD, fn_sw_e, 4);
            printf("fn_sw_e = %d\n", fn_sw_e);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_sw_pc"); //"fn_sw_pc"
    if (NULL != pSubSubSub)
    {
        if ((uint32_t)pSubSubSub->valueint != fn_sw_pc)
        {
            fn_sw_pc = (uint32_t)pSubSubSub->valueint;
            E2P_WriteLenByte(FN_ELE_QUAN_ADD, fn_sw_pc, 4);
            printf("fn_sw_pc = %d\n", fn_sw_pc);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "fn_sw_on"); //"fn_sw_pc"
    if (NULL != pSubSubSub)
    {
        if ((uint32_t)pSubSubSub->valueint != fn_sw_on)
        {
            fn_sw_on = (uint32_t)pSubSubSub->valueint;
            E2P_WriteLenByte(FN_SW_ON_ADD, fn_sw_on, 4);
            printf("fn_sw_on = %d\n", fn_sw_on);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "cg_data_led"); //"cg_data_led"
    if (NULL != pSubSubSub)
    {

        if ((uint8_t)pSubSubSub->valueint != cg_data_led)
        {
            cg_data_led = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(CG_DATA_LED_ADD, cg_data_led);
            printf("cg_data_led = %d\n", cg_data_led);
            // if (cg_data_led == 0)
            // {
            //     Turn_Off_LED();
            // }
            // else
            // {
            //     Turn_ON_LED();
            // }
        }
    }

    // pSubSubSub = cJSON_GetObjectItem(pJsonJson, "net_mode"); //"net_mode"
    // if (NULL != pSubSubSub)
    // {
    //     if ((uint8_t)pSubSubSub->valueint != net_mode)
    //     {

    //         net_mode = (uint8_t)pSubSubSub->valueint;
    //         E2P_WriteOneByte(NET_MODE_ADD, net_mode); //写入net_mode
    //         printf("net_mode = %d\n", net_mode);
    //     }
    // }

    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "de_sw_s"); //"de_sw_s"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != de_sw_s)
        {
            de_sw_s = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(DE_SWITCH_STA_ADD, de_sw_s); //写入net_mode
            printf("de_sw_s = %d\n", de_sw_s);
        }
    }
    cJSON_Delete(pJsonJson);
    // fn_set_flag = E2P_ReadOneByte(FN_SET_FLAG_ADD);
    // if (fn_set_flag != 1)
    // {
    //     fn_set_flag = 1;
    //     E2P_WriteOneByte(FN_SET_FLAG_ADD, fn_set_flag);
    // }

    return 1;
}

//解析蓝牙数据包
int32_t parse_objects_bluetooth(char *blu_json_data)
{
    cJSON *cjson_blu_data_parse = NULL;
    cJSON *cjson_blu_data_parse_command = NULL;
    printf("start_ble_parse_json：\r\n%s\n", blu_json_data);

    char *resp_val = NULL;
    resp_val = strstr(blu_json_data, "{");
    if (resp_val == NULL)
    {
        ESP_LOGE("JSON", "DATA NO JSON");
        return 0;
    }
    cjson_blu_data_parse = cJSON_Parse(resp_val);

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
        char *Json_temp;
        Json_temp = cJSON_PrintUnformatted(cjson_blu_data_parse);
        ParseTcpUartCmd(Json_temp);
        free(Json_temp);
    }
    cJSON_Delete(cjson_blu_data_parse);
    return 1;
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
    char *resp_val = NULL;
    resp_val = strstr(http_json_data, "{\"result\":\"success\",");
    if (resp_val == NULL)
    {
        ESP_LOGE("JSON", "DATA NO JSON");
        return 0;
    }
    json_data_parse = cJSON_Parse(resp_val);

    if (json_data_parse == NULL)
    {
        // printf("Json Formatting error3\n");
        ESP_LOGE(TAG, "%s", http_json_data);
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

            Parse_metadata(json_data_parse_channel_metadata->valuestring);

            //写入API-KEY
            if (strcmp(ApiKey, json_data_parse_channel_channel_write_key->valuestring) != 0)
            {
                strcpy(ApiKey, json_data_parse_channel_channel_write_key->valuestring);
                printf("api_key:%s\n", ApiKey);
                // sprintf(ApiKey, "%s%c", json_data_parse_channel_channel_write_key->valuestring, '\0');
                E2P_Write(API_KEY_ADD, (uint8_t *)ApiKey, API_KEY_LEN);
            }

            //写入channelid
            if (strcmp(ChannelId, json_data_parse_channel_channel_id_value->valuestring) != 0)
            {
                strcpy(ChannelId, json_data_parse_channel_channel_id_value->valuestring);
                printf("ChannelId:%s\n", ChannelId);
                // sprintf(ApiKey, "%s%c", json_data_parse_channel_channel_write_key->valuestring, '\0');
                E2P_Write(CHANNEL_ID_ADD, (uint8_t *)ChannelId, CHANNEL_ID_LEN);
            }

            //写入user_id
            if (strcmp(USER_ID, json_data_parse_channel_user_id->valuestring) != 0)
            {
                strcpy(USER_ID, json_data_parse_channel_user_id->valuestring);
                printf("USER_ID:%s\n", USER_ID);
                // sprintf(ApiKey, "%s%c", json_data_parse_channel_channel_write_key->valuestring, '\0');
                E2P_Write(USER_ID_ADD, (uint8_t *)USER_ID, USER_ID_LEN);
            }
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
    // cJSON *json_data_parse_errorcode = NULL;

    char *resp_val = NULL;
    resp_val = strstr(http_json_data, "{\"result\":");
    if (resp_val == NULL)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        return 0;
    }
    json_data_parse = cJSON_Parse(resp_val);

    if (json_data_parse == NULL)
    {

        // printf("Json Formatting error3\n");
        ESP_LOGE(TAG, "%s", http_json_data);
        cJSON_Delete(json_data_parse);
        return 0;
    }
    else
    {
        json_data_parse_value = cJSON_GetObjectItem(json_data_parse, "metadata");
        if (json_data_parse_value != NULL)
        {
            // printf("metadata: %s\n", json_data_parse_value->valuestring);
            Parse_metadata(json_data_parse_value->valuestring);
        }

        json_data_parse_value = cJSON_GetObjectItem(json_data_parse, "sensors"); //sensors
        if (NULL != json_data_parse_value)
        {
            Parse_fields_num(json_data_parse_value->valuestring); //parse sensors
        }

        json_data_parse_value = cJSON_GetObjectItem(json_data_parse, "command"); //
        if (NULL != json_data_parse_value)
        {
            char *mqtt_json;
            mqtt_json = cJSON_PrintUnformatted(json_data_parse_value);
            ESP_LOGI(TAG, "%s", mqtt_json);
            parse_objects_mqtt(mqtt_json, false); //parse mqtt
            free(mqtt_json);
        }

        json_data_parse_value = cJSON_GetObjectItem(json_data_parse, "cali"); //cali
        if (NULL != json_data_parse_value)
        {
            // printf("cali: %s\n", json_data_parse_value->valuestring);
            Parse_cali_dat(json_data_parse_value->valuestring); //parse cali
        }
    }

    cJSON_Delete(json_data_parse);
    return 1;
}

esp_err_t parse_objects_heart(char *json_data)
{
    cJSON *json_data_parse = NULL;
    cJSON *json_data_parse_value = NULL;

    char *resp_val = NULL;
    resp_val = strstr(json_data, "{\"result\":\"success\",");
    if (resp_val == NULL)
    {
        ESP_LOGE("JSON", "DATA NO JSON");
        return 0;
    }
    json_data_parse = cJSON_Parse(resp_val);

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

//解析MQTT指令
//sw_flag ：false,忽略开关执行指令
esp_err_t parse_objects_mqtt(char *mqtt_json_data, bool sw_flag)
{
    cJSON *json_data_parse = NULL;

    char *resp_val = NULL;
    resp_val = strstr(mqtt_json_data, "{\"command_id\":");
    if (resp_val == NULL)
    {
        // ESP_LOGE("JSON", "DATA NO JSON");
        return 0;
    }
    json_data_parse = cJSON_Parse(resp_val);

    if (json_data_parse == NULL) //如果数据包不为JSON则退出
    {
        printf("Json Formatting error5\n");
        cJSON_Delete(json_data_parse);
        return 0;
    }

    cJSON *pSubSubSub = cJSON_GetObjectItem(json_data_parse, "command_id"); //
    if (pSubSubSub != NULL)
    {
        memset(mqtt_json_s.mqtt_command_id, 0, sizeof(mqtt_json_s.mqtt_command_id));
        strncpy(mqtt_json_s.mqtt_command_id, pSubSubSub->valuestring, strlen(pSubSubSub->valuestring));
        post_status = POST_NORMAL;
        // if (Binary_dp != NULL)
        // {
        //     xTaskNotifyGive(Binary_dp);
        // }
    }

    pSubSubSub = cJSON_GetObjectItem(json_data_parse, "command_string"); //
    if (pSubSubSub != NULL)
    {
        // ESP_LOGI(TAG, "command_string=%s", pSubSubSub->valuestring);
        ParseTcpUartCmd(pSubSubSub->valuestring);

        cJSON *json_data_parse_1 = cJSON_Parse(pSubSubSub->valuestring);
        if (json_data_parse_1 != NULL)
        {
            pSubSubSub = cJSON_GetObjectItem(json_data_parse_1, "action"); //
            if (pSubSubSub != NULL)
            {
                if (strcmp(pSubSubSub->valuestring, "ota") == 0)
                {
                    pSubSubSub = cJSON_GetObjectItem(json_data_parse_1, "url"); //
                    if (pSubSubSub != NULL)
                    {
                        strcpy(mqtt_json_s.mqtt_ota_url, pSubSubSub->valuestring);
                        ESP_LOGI(TAG, "OTA_URL=%s", mqtt_json_s.mqtt_ota_url);

                        pSubSubSub = cJSON_GetObjectItem(json_data_parse_1, "size"); //
                        if (pSubSubSub != NULL)
                        {
                            mqtt_json_s.mqtt_file_size = (uint32_t)pSubSubSub->valuedouble;
                            ESP_LOGI(TAG, "OTA_SIZE=%d", mqtt_json_s.mqtt_file_size);

                            pSubSubSub = cJSON_GetObjectItem(json_data_parse_1, "version"); //
                            if (pSubSubSub != NULL)
                            {
                                if (strcmp(pSubSubSub->valuestring, FIRMWARE) != 0) //与当前 版本号 对比
                                {
                                    ESP_LOGI(TAG, "OTA_VERSION=%s", pSubSubSub->valuestring);
                                    ota_start(); //启动OTA
                                }
                            }
                        }
                    }
                }
                else if (strcmp(pSubSubSub->valuestring, "command") == 0 && sw_flag == true)
                {
                    pSubSubSub = cJSON_GetObjectItem(json_data_parse_1, "set_state");
                    if (pSubSubSub != NULL)
                    {
                        Switch_Relay(pSubSubSub->valueint);
                    }
                }
            }
        }
        cJSON_Delete(json_data_parse_1);
    }
    cJSON_Delete(json_data_parse);

    return 1;
}

uint16_t Create_Status_Json(char *status_buff, bool filed_flag)
{
    uint8_t mac_sys[6] = {0};
    char *ssid64_buff;
    esp_read_mac(mac_sys, 0); //获取芯片内部默认出厂MAC

    if (filed_flag == true)
    {
        char *field_buff, *cali_buff;
        field_buff = (char *)malloc(FILED_BUFF_SIZE);
        cali_buff = (char *)malloc(CALI_BUFF_SIZE);
        memset(field_buff, 0, FILED_BUFF_SIZE);
        memset(cali_buff, 0, CALI_BUFF_SIZE);
        Create_fields_num(field_buff);
        Create_cali_buf(cali_buff);

        if (net_mode == NET_WIFI)
        {
            ssid64_buff = (char *)malloc(64);
            memset(ssid64_buff, 0, 64);
            base64_encode(wifi_data.wifi_ssid, strlen(wifi_data.wifi_ssid), ssid64_buff, 64);

            sprintf(status_buff, "],\"status\":\"mac=%02x:%02x:%02x:%02x:%02x:%02x\",\"ssid_base64\":\"%s\",\"sensors\":[%s],\"cali\":[%s]}",
                    mac_sys[0],
                    mac_sys[1],
                    mac_sys[2],
                    mac_sys[3],
                    mac_sys[4],
                    mac_sys[5],
                    ssid64_buff,
                    field_buff,
                    cali_buff);
            free(ssid64_buff);
        }
        else
        {
            sprintf(status_buff, "],\"status\":\"mac=%02x:%02x:%02x:%02x:%02x:%02x,ICCID=%s\",\"sensors\":[%s],\"cali\":[%s]}",
                    mac_sys[0],
                    mac_sys[1],
                    mac_sys[2],
                    mac_sys[3],
                    mac_sys[4],
                    mac_sys[5],
                    ICCID,
                    field_buff,
                    cali_buff);
        }
        free(field_buff);
        free(cali_buff);
    }
    else
    {
        if (net_mode == NET_WIFI)
        {
            ssid64_buff = (char *)malloc(64);
            memset(ssid64_buff, 0, 64);
            base64_encode(wifi_data.wifi_ssid, strlen(wifi_data.wifi_ssid), ssid64_buff, 64);

            sprintf(status_buff, "],\"status\":\"mac=%02x:%02x:%02x:%02x:%02x:%02x\",\"ssid_base64\":\"%s\"}",
                    mac_sys[0],
                    mac_sys[1],
                    mac_sys[2],
                    mac_sys[3],
                    mac_sys[4],
                    mac_sys[5],
                    ssid64_buff);
            free(ssid64_buff);
        }
        else
        {
            sprintf(status_buff, "],\"status\":\"mac=%02x:%02x:%02x:%02x:%02x:%02x,ICCID=%s\"}",
                    mac_sys[0],
                    mac_sys[1],
                    mac_sys[2],
                    mac_sys[3],
                    mac_sys[4],
                    mac_sys[5],
                    ICCID);
        }
    }

    return strlen(status_buff);
}

void Create_NET_Json(void)
{

    if ((xEventGroupGetBits(Net_sta_group) & TIME_CAL_BIT) == TIME_CAL_BIT)
    {
        char *filed_buff;
        char *OutBuffer;
        char *time_buff;
        float ec_rssi_val;
        // uint8_t *SaveBuffer;
        uint16_t len = 0;
        cJSON *pJsonRoot;
        wifi_ap_record_t wifidata_t;

        filed_buff = (char *)malloc(9);
        time_buff = (char *)malloc(24);
        Server_Timer_SEND(time_buff);
        pJsonRoot = cJSON_CreateObject();
        cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)time_buff);

        if ((xEventGroupGetBits(Net_sta_group) & ACTIVED_BIT) == ACTIVED_BIT)
        {
            if (net_mode == NET_WIFI)
            {
                esp_wifi_sta_get_ap_info(&wifidata_t);
                snprintf(filed_buff, 9, "field%d", rssi_w_f_num);
                cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(wifidata_t.rssi));
            }
            else
            {
                EC20_Get_Rssi(&ec_rssi_val);
                snprintf(filed_buff, 9, "field%d", rssi_g_f_num);
                cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(ec_rssi_val));
            }
        }
        // cJSON_AddItemToObject(pJsonRoot, "field1", cJSON_CreateNumber(mqtt_json_s.mqtt_switch_status));

        OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
        cJSON_Delete(pJsonRoot);                       //delete cjson root
        len = strlen(OutBuffer);
        printf("len:%d\n%s\n", len, OutBuffer);
        // Send_Mqtt((uint8_t *)OutBuffer, len);
        xSemaphoreTake(Cache_muxtex, -1);
        DataSave((uint8_t *)OutBuffer, len);
        xSemaphoreGive(Cache_muxtex);
        free(OutBuffer);
        free(filed_buff);
        free(time_buff);
    }
}

void Create_Switch_Json(void)
{
    if ((xEventGroupGetBits(Net_sta_group) & TIME_CAL_BIT) == TIME_CAL_BIT)
    {
        char *OutBuffer;
        char *time_buff;
        uint16_t len = 0;
        cJSON *pJsonRoot;
        time_buff = (char *)malloc(24);
        Server_Timer_SEND(time_buff);

        pJsonRoot = cJSON_CreateObject();
        cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)time_buff);

        cJSON_AddItemToObject(pJsonRoot, "field1", cJSON_CreateNumber(mqtt_json_s.mqtt_switch_status));

        OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
        cJSON_Delete(pJsonRoot);                       //delete cjson root
        len = strlen(OutBuffer);
        printf("len:%d\n%s\n", len, OutBuffer);
        // Send_Mqtt((uint8_t *)OutBuffer, len);
        xSemaphoreTake(Cache_muxtex, -1);
        DataSave((uint8_t *)OutBuffer, len);
        xSemaphoreGive(Cache_muxtex);
        free(OutBuffer);
        free(time_buff);
    }
}

/*******************************************************************************
// create sensors fields num 
*******************************************************************************/
void Create_fields_num(char *read_buf)
{
    char *out_buf;
    uint16_t data_len;
    cJSON *pJsonRoot;

    pJsonRoot = cJSON_CreateObject();

    cJSON_AddNumberToObject(pJsonRoot, "sw_s", sw_s_f_num);         //sw_s_f_num
    cJSON_AddNumberToObject(pJsonRoot, "sw_v", sw_v_f_num);         //sw_v_f_num
    cJSON_AddNumberToObject(pJsonRoot, "sw_c", sw_c_f_num);         //sw_c_f_num
    cJSON_AddNumberToObject(pJsonRoot, "sw_p", sw_p_f_num);         //sw_p_f_num
    cJSON_AddNumberToObject(pJsonRoot, "sw_pc", sw_pc_f_num);       //sw_pc_f_num
    cJSON_AddNumberToObject(pJsonRoot, "rssi_w", rssi_w_f_num);     //rssi_w_f_num
    cJSON_AddNumberToObject(pJsonRoot, "rssi_g", rssi_g_f_num);     //rssi_g_f_num
    cJSON_AddNumberToObject(pJsonRoot, "r1_light", r1_light_f_num); //r1_light_f_num
    cJSON_AddNumberToObject(pJsonRoot, "r1_th_t", r1_th_t_f_num);   //r1_th_t_f_num
    cJSON_AddNumberToObject(pJsonRoot, "r1_th_h", r1_th_h_f_num);   //r1_th_h_f_num
    cJSON_AddNumberToObject(pJsonRoot, "r1_sth_t", r1_sth_t_f_num); //r1_sth_t_f_num
    cJSON_AddNumberToObject(pJsonRoot, "r1_sth_h", r1_sth_h_f_num); //r1_sth_h_f_num
    cJSON_AddNumberToObject(pJsonRoot, "e1_t", e1_t_f_num);         //e1_t_f_num
    cJSON_AddNumberToObject(pJsonRoot, "r1_t", r1_t_f_num);         //r1_t_f_num
    cJSON_AddNumberToObject(pJsonRoot, "r1_ws", r1_ws_f_num);       //r1_ws_f_num
    cJSON_AddNumberToObject(pJsonRoot, "r1_co2", r1_co2_f_num);     //r1_co2_f_num
    cJSON_AddNumberToObject(pJsonRoot, "r1_ph", r1_ph_f_num);       //r1_ph_f_num
    cJSON_AddNumberToObject(pJsonRoot, "r1_co2_t", r1_co2_t_f_num); //r1_co2_t_f_num
    cJSON_AddNumberToObject(pJsonRoot, "r1_co2_h", r1_co2_h_f_num); //r1_co2_h_f_num
    cJSON_AddNumberToObject(pJsonRoot, "sw_on", sw_on_f_num);       //sw_on_f_num

    out_buf = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
    data_len = strlen(out_buf);
    cJSON_Delete(pJsonRoot); //delete cjson root
    memcpy(read_buf, out_buf, data_len);
    free(out_buf);
}

/*******************************************************************************
// create cali num 
*******************************************************************************/
void Create_cali_buf(char *read_buf)
{
    char *out_buf;
    cJSON *pJsonRoot;

    pJsonRoot = cJSON_CreateObject();

    for (uint8_t i = 0; i < 40; i++)
    {
        cJSON_AddNumberToObject(pJsonRoot, f_cali_str[i], f_cali_u[i].val); //sw_s_f_num
    }

    out_buf = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
    cJSON_Delete(pJsonRoot);                     //delete cjson root
    strcpy(read_buf, out_buf);
    free(out_buf);
}

esp_err_t ParseTcpUartCmd(char *pcCmdBuffer)
{
    // char send_buf[128] = {0};
    // sprintf(send_buf, "{\"status\":0,\"code\": 0}");
    if (NULL == pcCmdBuffer) //null
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        return ESP_FAIL;
    }

    cJSON *pJson = cJSON_Parse(pcCmdBuffer); //parse json data
    if (NULL == pJson)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
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
                    // E2prom_set_defaul(false);
                    pSub = cJSON_GetObjectItem(pJson, "ProductID"); //"ProductID"
                    if (NULL != pSub)
                    {
                        printf("ProductID= %s, len= %d\n", pSub->valuestring, strlen(pSub->valuestring));
                        E2P_Write(PRODUCT_ID_ADDR, (uint8_t *)pSub->valuestring, PRODUCT_ID_LEN); //save ProductID
                    }

                    pSub = cJSON_GetObjectItem(pJson, "SeriesNumber"); //"SeriesNumber"
                    if (NULL != pSub)
                    {
                        printf("SeriesNumber= %s, len=%d\n", pSub->valuestring, strlen(pSub->valuestring));
                        E2P_Write(SERISE_NUM_ADDR, (uint8_t *)pSub->valuestring, SERISE_NUM_LEN); //save
                    }

                    pSub = cJSON_GetObjectItem(pJson, "Host"); //"Host"
                    if (NULL != pSub)
                    {
                        printf("Host= %s, len=%d\n", pSub->valuestring, strlen(pSub->valuestring));
                        E2P_Write(WEB_HOST_ADD, (uint8_t *)pSub->valuestring, WEB_HOST_LEN); //save
                    }

                    pSub = cJSON_GetObjectItem(pJson, "Port"); //"WEB PORT"
                    if (NULL != pSub)
                    {

                        printf("Port= %s, len=%d\n", pSub->valuestring, strlen(pSub->valuestring));
                        E2P_Write(WEB_PORT_ADD, (uint8_t *)pSub->valuestring, 5); //save
                    }

                    pSub = cJSON_GetObjectItem(pJson, "MqttHost"); //"mqtt Host"
                    if (NULL != pSub)
                    {
                        printf("MqttHost= %s, len=%d\n", pSub->valuestring, strlen(pSub->valuestring));
                        E2P_Write(MQTT_HOST_ADD, (uint8_t *)pSub->valuestring, WEB_HOST_LEN); //save
                    }

                    pSub = cJSON_GetObjectItem(pJson, "MqttPort"); //"Host"
                    if (NULL != pSub)
                    {
                        printf("MqttPort= %s, len=%d\n", pSub->valuestring, strlen(pSub->valuestring));
                        E2P_Write(MQTT_PORT_ADD, (uint8_t *)pSub->valuestring, 5); //save
                    }

                    printf("{\"status\":0,\"code\": 0}");

                    vTaskDelay(3000 / portTICK_RATE_MS);
                    cJSON_Delete(pJson);
                    esp_restart(); //芯片复位 函数位于esp_system.h

                    return ESP_OK;
                }
                else
                {
                    //密码错误
                    printf("{\"status\":1,\"code\": 101}\r\n");
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
                E2P_Write(WIFI_SSID_ADD, (uint8_t *)wifi_data.wifi_ssid, sizeof(wifi_data.wifi_ssid));
                printf("WIFI_SSID = %s\r\n", pSub->valuestring);
                net_mode = NET_WIFI;
                E2P_WriteOneByte(NET_MODE_ADD, net_mode); //写入net_mode
            }

            pSub = cJSON_GetObjectItem(pJson, "password"); //"password"
            if (NULL != pSub)
            {
                bzero(wifi_data.wifi_pwd, sizeof(wifi_data.wifi_pwd));
                strcpy(wifi_data.wifi_pwd, pSub->valuestring);
                E2P_Write(WIFI_PASSWORD_ADD, (uint8_t *)wifi_data.wifi_pwd, sizeof(wifi_data.wifi_pwd));
                printf("WIFI_PWD = %s\r\n", pSub->valuestring);
            }

            pSub = cJSON_GetObjectItem(pJson, "apn"); //"apns"
            if (NULL != pSub)
            {
                bzero(SIM_APN, sizeof(SIM_APN));
                strcpy(SIM_APN, pSub->valuestring);
                E2P_Write(APN_ADDR, (uint8_t *)SIM_APN, sizeof(SIM_APN));
                printf("apn = %s\r\n", SIM_APN);
                net_mode = NET_4G;
                E2P_WriteOneByte(NET_MODE_ADD, net_mode); //写入net_mode
            }

            pSub = cJSON_GetObjectItem(pJson, "user"); //"user"
            if (NULL != pSub)
            {
                bzero(SIM_USER, sizeof(SIM_USER));
                strcpy(SIM_USER, pSub->valuestring);
                E2P_Write(BEARER_USER_ADDR, (uint8_t *)SIM_USER, sizeof(SIM_USER));
                printf("user = %s\r\n", SIM_USER);
            }

            pSub = cJSON_GetObjectItem(pJson, "pwd"); //"pwd"
            if (NULL != pSub)
            {
                bzero(SIM_PWD, sizeof(SIM_PWD));
                strcpy(SIM_PWD, pSub->valuestring);
                E2P_Write(BEARER_PWD_ADDR, (uint8_t *)SIM_PWD, sizeof(SIM_PWD));
                printf("pwd = %s\r\n", SIM_PWD);
            }
            Net_Switch();
            cJSON_Delete(pJson); //delete pJson
            printf("{\"status\":0,\"code\": 0}\r\n");
            //重置网络

            return 1;
        }
        //{"command":"SetupHost","Host":"api.ubibot.io","Port":"80","MqttHost":"mqtt.ubibot.io","MqttPort":"1883"}
        else if (!strcmp((char const *)pSub->valuestring, "SetupHost")) //Command:SetupWifi
        {
            pSub = cJSON_GetObjectItem(pJson, "Host"); //"Host"
            if (NULL != pSub)
            {
                ESP_LOGI(TAG, "Host= %s, len=%d\n", pSub->valuestring, strlen(pSub->valuestring));
                E2P_Write(WEB_HOST_ADD, (uint8_t *)pSub->valuestring, WEB_HOST_LEN); //save SeriesNumber
            }

            pSub = cJSON_GetObjectItem(pJson, "Port"); //"WEB PORT"
            if (NULL != pSub)
            {
                printf("Port= %s, len=%d\n", pSub->valuestring, strlen(pSub->valuestring));
                E2P_Write(WEB_PORT_ADD, (uint8_t *)pSub->valuestring, 5); //save
            }

            pSub = cJSON_GetObjectItem(pJson, "MqttHost"); //"mqtt Host"
            if (NULL != pSub)
            {
                printf("MqttHost= %s, len=%d\n", pSub->valuestring, strlen(pSub->valuestring));
                E2P_Write(MQTT_HOST_ADD, (uint8_t *)pSub->valuestring, WEB_HOST_LEN); //save
            }

            pSub = cJSON_GetObjectItem(pJson, "MqttPort"); //"Host"
            if (NULL != pSub)
            {
                printf("MqttPort= %s, len=%d\n", pSub->valuestring, strlen(pSub->valuestring));
                E2P_Write(MQTT_PORT_ADD, (uint8_t *)pSub->valuestring, 5); //save
            }

            printf("{\"status\":0,\"code\": 0}");

            vTaskDelay(3000 / portTICK_RATE_MS);
            cJSON_Delete(pJson);
            esp_restart(); //芯片复位 函数位于esp_system.h

            return ESP_OK;
        }

        //{"command":"ReadProduct"}
        else if (!strcmp((char const *)pSub->valuestring, "ReadProduct")) //Command:ReadProduct
        {
            char mac_buff[18];
            char *json_temp;
            uint8_t mac_sys[6] = {0};
            cJSON *root = cJSON_CreateObject();

            // E2P_Read(SERISE_NUM_ADDR, (uint8_t *)SerialNum, SERISE_NUM_LEN);
            // E2P_Read(PRODUCT_ID_ADDR, (uint8_t *)ProductId, PRODUCT_ID_LEN);
            // E2P_Read(WEB_HOST_ADD, (uint8_t *)WEB_SERVER, WEB_HOST_LEN);
            // E2P_Read(CHANNEL_ID_ADD, (uint8_t *)ChannelId, CHANNEL_ID_LEN);
            // E2P_Read(USER_ID_ADD, (uint8_t *)USER_ID, USER_ID_LEN);
            // E2P_Read(WEB_PORT_ADD, (uint8_t *)WEB_PORT, 5);
            // E2P_Read(MQTT_HOST_ADD, (uint8_t *)MQTT_SERVER, USER_ID_LEN);
            // E2P_Read(MQTT_PORT_ADD, (uint8_t *)MQTT_PORT, 5);

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
            cJSON_AddItemToObject(root, "Port", cJSON_CreateString(WEB_PORT));
            cJSON_AddItemToObject(root, "MqttHost", cJSON_CreateString(MQTT_SERVER));
            cJSON_AddItemToObject(root, "MqttPort", cJSON_CreateString(MQTT_PORT));
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
        //{"command":"CheckSensors"}
        else if (!strcmp((char const *)pSub->valuestring, "CheckSensors"))
        {
            cJSON *root = cJSON_CreateObject();
            char *json_temp;

            bool result_flag = true;
            bool RS484_flag = false;
            bool ENERGY_flag = false;
            bool DS18B20_flag = false;

            xTaskNotifyGive(Binary_485_th);
            xTaskNotifyGive(Binary_ext);
            xTaskNotifyGive(Binary_energy);

            if ((xEventGroupWaitBits(Net_sta_group, RS485_CHECK_BIT, true, true, 1000 / portTICK_RATE_MS) & RS485_CHECK_BIT) == RS485_CHECK_BIT)
            {
                // printf("{\"result\":\"RS485 OK\",\"temp_val\":%d,\"humi_val\":%d}\r\n", (int)ext_tem, (int)ext_hum);
                RS484_flag = true;
            }
            else
            {
                // printf("{\"result\":\"RS485 ERROR\"}\r\n");
                result_flag = false;
                RS484_flag = false;
            }

            if ((xEventGroupWaitBits(Net_sta_group, CSE_CHECK_BIT, true, true, 1000 / portTICK_RATE_MS) & CSE_CHECK_BIT) == CSE_CHECK_BIT)
            {
                // printf("{\"result\":\"ENERGY OK\"}\r\n");
                ENERGY_flag = true;
            }
            else
            {
                // printf("{\"result\":\"ENERGY ERROR\"}\r\n");
                result_flag = false;
                ENERGY_flag = false;
            }

            if ((xEventGroupWaitBits(Net_sta_group, DS18B20_CHECK_BIT, true, true, 5000 / portTICK_RATE_MS) & DS18B20_CHECK_BIT) == DS18B20_CHECK_BIT)
            {
                // printf("{\"result\":\"DS18B20 OK\",\"ext_temp_val\":%d}\r\n", (int)DS18B20_TEM);
                DS18B20_flag = true;
            }
            else
            {
                // printf("{\"result\":\"DS18B20 ERROR\"}\r\n");
                result_flag = false;
                DS18B20_flag = false;
            }

            if ((E2P_FLAG & E2P_FLAG) != true)
            {
                result_flag = false;
            }

            //构建返回结果
            if (result_flag == true)
            {
                cJSON_AddStringToObject(root, "result", "OK");
            }
            else
            {
                cJSON_AddStringToObject(root, "result", "ERROR");
            }

            if (RS484_flag == true)
            {
                cJSON_AddNumberToObject(root, "temp_val", (int)ext_tem);
                cJSON_AddNumberToObject(root, "humi_val", (int)ext_hum);
            }
            else
            {
                cJSON_AddStringToObject(root, "RS485", "ERROR");
            }

            if (ENERGY_flag == true)
            {
                cJSON_AddStringToObject(root, "ENERGY", "OK");
            }
            else
            {
                cJSON_AddStringToObject(root, "ENERGY", "ERROR");
            }

            if (DS18B20_flag == true)
            {
                cJSON_AddNumberToObject(root, "ext_temp_val", (int)DS18B20_TEM);
            }
            else
            {
                cJSON_AddStringToObject(root, "DS18B20", "ERROR");
            }

            if (E2P_FLAG == true)
            {
                cJSON_AddStringToObject(root, "e2prom", "OK");
            }
            else
            {
                cJSON_AddStringToObject(root, "e2prom", "ERROR");
            }

            if (FLASH_FLAG == true)
            {
                cJSON_AddStringToObject(root, "flash", "OK");
            }
            else
            {
                cJSON_AddStringToObject(root, "flash", "ERROR");
            }

            json_temp = cJSON_PrintUnformatted(root);
            printf("%s\n", json_temp);
            cJSON_Delete(root); //delete pJson
            free(json_temp);
        }
        //{"command":"CheckModule"}
        else if (!strcmp((char const *)pSub->valuestring, "CheckModule"))
        {
            Check_Module();
        }
        //{"command":"ScanWifiList"}
        else if (!strcmp((char const *)pSub->valuestring, "ScanWifiList"))
        {
            Scan_Wifi();
        }
        //{"command":"reboot"}
        else if (!strcmp((char const *)pSub->valuestring, "reboot"))
        {
            esp_restart();
        }
        //{"command":"AllReset"}
        else if (!strcmp((char const *)pSub->valuestring, "AllReset"))
        {
            E2prom_set_defaul(false);
        }
    }

    cJSON_Delete(pJson); //delete pJson

    return ESP_FAIL;
}

/*******************************************************************************
// parse sensors fields num
*******************************************************************************/
static short Parse_fields_num(char *ptrptr)
{
    if (NULL == ptrptr)
    {
        return ESP_FAIL;
    }
    cJSON *pJsonJson = cJSON_Parse(ptrptr);
    if (NULL == pJsonJson)
    {
        cJSON_Delete(pJsonJson); //delete pJson
        return ESP_FAIL;
    }

    cJSON *pSubSubSub = cJSON_GetObjectItem(pJsonJson, "sw_s"); //"sw_s"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != sw_s_f_num)
        {
            sw_s_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(SW_S_F_NUM_ADDR, sw_s_f_num);
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "sw_v"); //"sw_v"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != sw_v_f_num)
        {
            sw_v_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(SW_V_F_NUM_ADDR, sw_v_f_num); //
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "sw_c"); //""
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != sw_c_f_num)
        {
            sw_c_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(SW_C_F_NUM_ADDR, sw_c_f_num); //
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "sw_p"); //""
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != sw_p_f_num)
        {
            sw_p_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(SW_P_F_NUM_ADDR, sw_p_f_num); //
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "sw_pc"); //""
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != sw_pc_f_num)
        {
            sw_pc_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(SW_PC_F_NUM_ADDR, sw_pc_f_num); //
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "sw_on"); //""
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != sw_on_f_num)
        {
            sw_on_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(SW_ON_F_NUM_ADDR, sw_on_f_num); //
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "rssi_w"); //"rssi_w"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != rssi_w_f_num)
        {
            rssi_w_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(RSSI_NUM_ADDR, rssi_w_f_num); //rssi_w_f_num
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "rssi_g"); //"rssi_g"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != rssi_g_f_num)
        {
            rssi_g_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(GPRS_RSSI_NUM_ADDR, rssi_g_f_num); //rssi_g_f_num
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "r1_light"); //"r1_light"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != r1_light_f_num)
        {
            r1_light_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(RS485_LIGHT_NUM_ADDR, r1_light_f_num); //r1_light_f_num
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "r1_th_t"); //"r1_th_t"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != r1_th_t_f_num)
        {
            r1_th_t_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(RS485_TEMP_NUM_ADDR, r1_th_t_f_num); //r1_th_t_f_num
        }
    }

    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "r1_th_h"); //"r1_th_h"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != r1_th_h_f_num)
        {
            r1_th_h_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(RS485_HUMI_NUM_ADDR, r1_th_h_f_num); //r1_th_h_f_num
        }
    }

    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "r1_sth_t"); //"r1_sth_t"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != r1_sth_t_f_num)
        {
            r1_sth_t_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(RS485_STEMP_NUM_ADDR, r1_sth_t_f_num); //r1_sth_t_f_num
        }
    }

    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "r1_sth_h"); //"r1_sth_h"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != r1_sth_h_f_num)
        {
            r1_sth_h_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(RS485_SHUMI_NUM_ADDR, r1_sth_h_f_num); //r1_sth_h_f_num
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "e1_t"); //"e1_t"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != e1_t_f_num)
        {
            e1_t_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(EXT_TEMP_NUM_ADDR, e1_t_f_num); //e1_t_f_num
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "r1_t"); //"r1_t"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != r1_t_f_num)
        {
            r1_t_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(RS485_T_NUM_ADDR, r1_t_f_num); //r1_t_f_num
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "r1_ws"); //"r1_ws"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != r1_ws_f_num)
        {
            r1_ws_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(RS485_WS_NUM_ADDR, r1_ws_f_num); //r1_ws_f_num
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "r1_co2"); //"r1_co2"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != r1_co2_f_num)
        {
            r1_co2_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(RS485_CO2_NUM_ADDR, r1_co2_f_num); //r1_co2_f_num
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "r1_ph"); //"r1_ph"
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != r1_ph_f_num)
        {
            r1_ph_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(RS485_PH_NUM_ADDR, r1_ph_f_num); //r1_ph_f_num
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "r1_co2_t"); //""
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != r1_co2_t_f_num)
        {
            r1_co2_t_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(RS485_CO2_T_NUM_ADDR, r1_co2_t_f_num); //
        }
    }
    pSubSubSub = cJSON_GetObjectItem(pJsonJson, "r1_co2_h"); //""
    if (NULL != pSubSubSub)
    {
        if ((uint8_t)pSubSubSub->valueint != r1_co2_h_f_num)
        {
            r1_co2_h_f_num = (uint8_t)pSubSubSub->valueint;
            E2P_WriteOneByte(RS485_CO2_H_NUM_ADDR, r1_co2_h_f_num); //
        }
    }

    cJSON_Delete(pJsonJson);

    return ESP_OK;
}

/*******************************************************************************
// parse sensors fields num
*******************************************************************************/
static short Parse_cali_dat(char *ptrptr)
{
    cJSON *pSubSubSub;

    if (NULL == ptrptr)
    {
        return ESP_FAIL;
    }
    cJSON *pJsonJson = cJSON_Parse(ptrptr);
    if (NULL == pJsonJson)
    {
        cJSON_Delete(pJsonJson); //delete pJson
        return ESP_FAIL;
    }

    for (uint8_t i = 0; i < 40; i++)
    {
        pSubSubSub = cJSON_GetObjectItem(pJsonJson, f_cali_str[i]); //
        if (NULL != pSubSubSub)
        {
            if ((float)pSubSubSub->valuedouble != f_cali_u[i].val)
            {
                f_cali_u[i].val = (float)pSubSubSub->valuedouble;
                E2P_WriteLenByte(5 * i + F1_A_ADDR, f_cali_u[i].dat, 4);
            }
        }
    }

    cJSON_Delete(pJsonJson);
    return ESP_OK;
}

//计算 filed 校准值
double Cali_filed(uint8_t filed_num, double filed_val)
{
    double filed_val_c;
    filed_val_c = (double)f_cali_u[(filed_num - 1) * 2].val * filed_val + f_cali_u[(filed_num - 1) * 2 + 1].val; //a*X+b

    return filed_val_c;
}

// get mid str 把src中 s1 到 s2之间的数据拷贝（包括s1不包括S2）到 sub中 ,返回 s2地址
char *mid(char *src, char *s1, char *s2, char *sub)
{
    char *sub1;
    char *sub2;
    uint16_t n;

    sub1 = strstr(src, s1);
    if (sub1 == NULL)
        return NULL;
    sub1 += strlen(s1);
    sub2 = strstr(sub1, s2);
    if (sub2 == NULL)
        return NULL;
    n = sub2 - sub1;
    strncpy(sub, sub1, n);
    sub[n] = 0;
    return sub2;
}

// return: NULL Not Found
//反向查找字符串 _MaxLen: 内存大小，_ReadLen:查找的长度
char *s_rstrstr(const char *_pBegin, int _MaxLen, int _ReadLen, const char *_szKey)
{
    if (NULL == _pBegin || NULL == _szKey || _MaxLen <= 0)
    {
        return NULL;
    }
    int i = _MaxLen - 1;
    int j = strlen(_szKey) - 1;
    int k = 0;
    // int s32CmpLen = strlen(_szKey);

    for (i = _MaxLen - 1; i >= (_MaxLen - _ReadLen); i--)
    {
        if (_pBegin[i] == _szKey[j])
        {
            for (j = strlen(_szKey) - 1, k = i; j >= 0; j--, k--)
            {
                if (_pBegin[k] != _szKey[j])
                {
                    j = strlen(_szKey) - 1;
                    break;
                }
                if (j == 0)
                {
                    return (char *)_pBegin + i - strlen(_szKey) + 1;
                }
            }
        }
    }

    return NULL;
}

// return: NULL Not Found
//buff中正向查找字符串 _ReadLen:查找的长度
//返回 要查找的字符串之后的地址
char *s_strstr(const char *_pBegin, int _ReadLen, int *first_len, const char *_szKey)
{
    if (NULL == _pBegin || NULL == _szKey || _ReadLen <= 0)
    {
        return NULL;
    }
    int i = 0, j = 0, k = 0;
    int s32CmpLen = strlen(_szKey);

    for (i = 0; i < _ReadLen; i++)
    {
        if (_pBegin[i] == _szKey[j])
        {
            for (j = 0, k = i; j < s32CmpLen; j++, k++)
            {
                if (_pBegin[k] != _szKey[j])
                {
                    j = 0;
                    break;
                }
                if (j == s32CmpLen - 1)
                {
                    if (first_len != NULL)
                    {
                        // printf("s_strstr,i=%d\n", i);
                        *first_len = i;
                    }
                    return (char *)_pBegin + i + s32CmpLen;
                }
            }
        }
    }

    return NULL;
}

//读取EEPROM中的metadata
void Read_Metadate_E2p(void)
{
    uint8_t Last_Switch_Status;
    de_sw_s = E2P_ReadOneByte(DE_SWITCH_STA_ADD); //上电开关默认状态
    switch (de_sw_s)
    {
    case 0:
        Switch_Relay(0);
        break;

    case 1:
        Switch_Relay(1);
        break;

    case 2:
        Last_Switch_Status = E2P_ReadOneByte(LAST_SWITCH_ADD);
        if (Last_Switch_Status <= 100)
        {
            Switch_Relay(E2P_ReadOneByte(LAST_SWITCH_ADD));
        }
        break;

    default:
        break;
    }

    fn_dp = E2P_ReadLenByte(FN_DP_ADD, 4);           //数据发送频率
    fn_485_t = E2P_ReadLenByte(FN_485_T_ADD, 4);     //
    fn_485_th = E2P_ReadLenByte(FN_485_TH_ADD, 4);   //
    fn_485_sth = E2P_ReadLenByte(FN_485_STH_ADD, 4); //485 土壤探头
    fn_485_ws = E2P_ReadLenByte(FN_485_WS_ADD, 4);   //
    fn_485_lt = E2P_ReadLenByte(FN_485_LT_ADD, 4);   //
    fn_485_co2 = E2P_ReadLenByte(FN_485_CO2_ADD, 4); //
    fn_ext = E2P_ReadLenByte(FN_EXT_ADD, 4);         //18b20
    fn_sw_e = E2P_ReadLenByte(FN_ENERGY_ADD, 4);     //电能信息：电压/电流/功率
    fn_sw_pc = E2P_ReadLenByte(FN_ELE_QUAN_ADD, 4);  //用电量统计
    cg_data_led = E2P_ReadOneByte(CG_DATA_LED_ADD);  //发送数据 LED状态 0关闭，1打开
    net_mode = E2P_ReadOneByte(NET_MODE_ADD);        //上网模式选择 0：自动模式 1：lan模式 2：wifi模式
    fn_sw_on = E2P_ReadLenByte(FN_SW_ON_ADD, 4);     //累积开启时长统计周期

    printf("E2P USAGE:%d\n", E2P_USAGED);

    printf("fn_dp:%d\n", fn_dp);
    printf("fn_485_t:%d\n", fn_485_t);
    printf("fn_485_th:%d\n", fn_485_th);
    printf("fn_485_sth:%d\n", fn_485_sth);
    printf("fn_485_ws:%d\n", fn_485_ws);
    printf("fn_485_lt:%d\n", fn_485_lt);
    printf("fn_485_co2:%d\n", fn_485_co2);
    printf("fn_ext:%d\n", fn_ext);
    printf("fn_sw_e:%d\n", fn_sw_e);
    printf("cg_data_led:%d\n", cg_data_led);
    printf("net_mode:%d\n", net_mode);
    printf("de_sw_s:%d\n", de_sw_s);
    printf("fn_sw_on:%d\n", fn_sw_on);
}

void Read_Product_E2p(void)
{
    printf("FIRMWARE=%s\n", FIRMWARE);
    E2P_Read(SERISE_NUM_ADDR, (uint8_t *)SerialNum, SERISE_NUM_LEN);
    printf("SerialNum=%s\n", SerialNum);
    E2P_Read(PRODUCT_ID_ADDR, (uint8_t *)ProductId, PRODUCT_ID_LEN);
    printf("ProductId=%s\n", ProductId);
    E2P_Read(WEB_HOST_ADD, (uint8_t *)WEB_SERVER, WEB_HOST_LEN);
    printf("WEB_SERVER=%s\n", WEB_SERVER);
    E2P_Read(WEB_PORT_ADD, (uint8_t *)WEB_PORT, 5);
    printf("WEB_PORT=%s\n", WEB_PORT);
    E2P_Read(MQTT_HOST_ADD, (uint8_t *)MQTT_SERVER, WEB_HOST_LEN);
    printf("MQTT_SERVER=%s\n", MQTT_SERVER);
    E2P_Read(MQTT_PORT_ADD, (uint8_t *)MQTT_PORT, 5);
    printf("MQTT_PORT=%s\n", MQTT_PORT);
    E2P_Read(CHANNEL_ID_ADD, (uint8_t *)ChannelId, CHANNEL_ID_LEN);
    printf("ChannelId=%s\n", ChannelId);
    E2P_Read(USER_ID_ADD, (uint8_t *)USER_ID, USER_ID_LEN);
    printf("USER_ID=%s\n", USER_ID);
    E2P_Read(API_KEY_ADD, (uint8_t *)ApiKey, API_KEY_LEN);
    printf("ApiKey=%s\n", ApiKey);
    E2P_Read(WIFI_SSID_ADD, (uint8_t *)wifi_data.wifi_ssid, sizeof(wifi_data.wifi_ssid));
    E2P_Read(WIFI_PASSWORD_ADD, (uint8_t *)wifi_data.wifi_pwd, sizeof(wifi_data.wifi_pwd));
    printf("wifi ssid=%s,wifi password=%s\n", wifi_data.wifi_ssid, wifi_data.wifi_pwd);

    E2P_Read(APN_ADDR, (uint8_t *)SIM_APN, sizeof(SIM_APN));
    E2P_Read(BEARER_USER_ADDR, (uint8_t *)SIM_USER, sizeof(SIM_USER));
    E2P_Read(BEARER_PWD_ADDR, (uint8_t *)SIM_PWD, sizeof(SIM_PWD));
    printf("APN=%s,USER=%s,PWD=%s\n", SIM_APN, SIM_USER, SIM_PWD);

    if (strlen(WEB_SERVER) == 0)
    {
        sprintf(WEB_SERVER, "api.ubibot.cn");
    }
    if (strlen(WEB_PORT) == 0)
    {
        sprintf(WEB_PORT, "80");
    }
    if (strlen(MQTT_SERVER) == 0)
    {
        sprintf(MQTT_SERVER, "mqtt.ubibot.cn");
    }
    if (strlen(MQTT_PORT) == 0)
    {
        sprintf(MQTT_PORT, "1883");
    }
}

void Read_Fields_E2p(void)
{
    uint8_t sensors_temp;

    if ((sensors_temp = E2P_ReadOneByte(SW_S_F_NUM_ADDR)) != 0)
    {
        sw_s_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(SW_V_F_NUM_ADDR)) != 0)
    {
        sw_v_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(SW_C_F_NUM_ADDR)) != 0)
    {
        sw_c_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(SW_P_F_NUM_ADDR)) != 0)
    {
        sw_p_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(SW_PC_F_NUM_ADDR)) != 0)
    {
        sw_pc_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(RSSI_NUM_ADDR)) != 0)
    {
        rssi_w_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(GPRS_RSSI_NUM_ADDR)) != 0)
    {
        rssi_g_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(RS485_LIGHT_NUM_ADDR)) != 0)
    {
        r1_light_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(RS485_TEMP_NUM_ADDR)) != 0)
    {
        r1_th_t_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(RS485_HUMI_NUM_ADDR)) != 0)
    {
        r1_th_h_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(RS485_STEMP_NUM_ADDR)) != 0)
    {
        r1_sth_t_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(RS485_SHUMI_NUM_ADDR)) != 0)
    {
        r1_sth_h_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(EXT_TEMP_NUM_ADDR)) != 0)
    {
        e1_t_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(RS485_T_NUM_ADDR)) != 0)
    {
        r1_t_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(RS485_WS_NUM_ADDR)) != 0)
    {
        r1_ws_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(RS485_CO2_NUM_ADDR)) != 0)
    {
        r1_co2_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(RS485_PH_NUM_ADDR)) != 0)
    {
        r1_ph_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(RS485_CO2_T_NUM_ADDR)) != 0)
    {
        r1_co2_t_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(RS485_CO2_H_NUM_ADDR)) != 0)
    {
        r1_co2_h_f_num = sensors_temp;
    }
    if ((sensors_temp = E2P_ReadOneByte(SW_ON_F_NUM_ADDR)) != 0)
    {
        sw_on_f_num = sensors_temp;
    }

    for (uint8_t i = 0; i < 40; i++)
    {
        f_cali_u[i].dat = E2P_ReadLenByte(5 * i + F1_A_ADDR, 4);
        if (i % 2 == 0 && f_cali_u[i].val == 0)
        {
            f_cali_u[i].val = 1;
        }

        printf("%s:%f\n", f_cali_str[i], f_cali_u[i].val);
    }

    printf("sw_s_f_num:%d\n", sw_s_f_num);
    printf("sw_v_f_num:%d\n", sw_v_f_num);
    printf("sw_c_f_num:%d\n", sw_c_f_num);
    printf("sw_p_f_num:%d\n", sw_p_f_num);
    printf("sw_pc_f_num:%d\n", sw_pc_f_num);
    printf("rssi_w_f_num:%d\n", rssi_w_f_num);
    printf("rssi_g_f_num:%d\n", rssi_g_f_num);
    printf("sw_on_f_num:%d\n", sw_on_f_num);
    printf("r1_light_f_num:%d\n", r1_light_f_num);
    printf("r1_th_t_f_num:%d\n", r1_th_t_f_num);
    printf("r1_th_h_f_num:%d\n", r1_th_h_f_num);
    printf("r1_sth_t_f_num:%d\n", r1_sth_t_f_num);
    printf("r1_sth_h_f_num:%d\n", r1_sth_h_f_num);
    printf("e1_t_f_num:%d\n", e1_t_f_num);
    printf("r1_t_f_num:%d\n", r1_t_f_num);
    printf("r1_ws_f_num:%d\n", r1_ws_f_num);
    printf("r1_co2_f_num:%d\n", r1_co2_f_num);
    printf("r1_ph_f_num:%d\n", r1_ph_f_num);
    printf("r1_co2_t_f_num:%d\n", r1_co2_t_f_num);
    printf("r1_co2_h_f_num:%d\n", r1_co2_h_f_num);
}