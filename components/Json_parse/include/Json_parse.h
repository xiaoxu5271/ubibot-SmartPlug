#ifndef __Json_parse
#define __Json_parse
#include <stdio.h>
#include "esp_err.h"

esp_err_t parse_objects_http_active(char *http_json_data);
esp_err_t parse_objects_bluetooth(char *blu_json_data);
esp_err_t parse_objects_mqtt(char *json_data);
esp_err_t parse_objects_heart(char *json_data);
esp_err_t parse_objects_http_respond(char *http_json_data);
esp_err_t ParseTcpUartCmd(char *pcCmdBuffer);

esp_err_t creat_object(void);

#define WORK_INIT 0X00       //初始化
#define WORK_AUTO 0x01       //平台自动模式
#define WORK_HAND 0x02       //网页版手动模式
#define WORK_HANDTOAUTO 0x03 //用于自动回复时执行一次自动控制指令
#define WORK_LOCAL 0x04      //本地计算控制模式
#define WORK_WAITLOCAL 0x05  //本地计算等待模式（用于状态机空闲状态）
#define WORK_WALLKEY 0X06    //本地墙壁开关控制模式
#define WORK_PROTECT 0X07    //风速和结霜保护
#define WORK_FIREINIT 0X08   //开机就火灾
#define WORK_FIRE 0x09       //火灾保护状态

#define NET_AUTO 0 //上网模式 自动
#define NET_LAN 1  //上网模式 网线
#define NET_WIFI 2 //上网模式 wifi

struct
{
    int mqtt_Voltage;
    double mqtt_Current;
    double mqtt_Power;
    double mqtt_Energy;
    bool mqtt_switch_status;

    char mqtt_command_id[32];
    char mqtt_string[256];
    char mqtt_Rssi[8];

    char mqtt_ota_url[128]; //OTA升级地址
    char mqtt_etx_tem[8];
    char mqtt_etx_hum[8];
    char mqtt_DS18B20_TEM[8];

} mqtt_json_s;

struct
{
    char wifi_ssid[36];
    char wifi_pwd[36];
} wifi_data;

struct
{
    char http_time[24];
} http_json_c;

typedef struct
{
    char creat_json_b[256];
    int creat_json_c;
} creat_json;

int read_bluetooth(void);
//creat_json *create_http_json(uint8_t post_status);
void create_http_json(creat_json *pCreat_json, uint8_t flag);
void Read_Metadate(void);
uint8_t Create_NET_Json(char *status_buff);

/************metadata 参数***********/
extern uint32_t fn_dp;       //数据发送频率
extern uint32_t fn_485_th;   //485温湿度探头
extern uint32_t fn_485_sth;  //485 土壤探头
extern uint32_t fn_ext;      //18b20
extern uint32_t fn_energy;   //电能信息：电压/电流/功率
extern uint32_t fn_ele_quan; //用电量统计
// extern uint32_t fn_sen;     //人感灵敏度
extern uint8_t cg_data_led; //发送数据 LED状态 0关闭，1打开
extern uint8_t net_mode;    //上网模式选择 0：自动模式 1：lan模式 2：wifi模式
/************************************/

#endif