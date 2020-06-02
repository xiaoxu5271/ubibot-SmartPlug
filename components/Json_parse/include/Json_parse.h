#ifndef __Json_parse
#define __Json_parse
#include <stdio.h>
#include "esp_err.h"
#include "E2prom.h"

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

#define NET_AUTO 2 //上网模式 自动
#define NET_4G 1   //上网模式 4G
#define NET_WIFI 0 //上网模式 wifi

#define FILED_BUFF_SIZE 350

struct
{
    // int mqtt_Voltage;
    // double mqtt_Current;
    // double mqtt_Power;
    // double mqtt_Energy;
    bool mqtt_switch_status;

    char mqtt_command_id[32];
    // char mqtt_string[256];
    // char mqtt_Rssi[8];

    char mqtt_ota_url[128]; //OTA升级地址
    uint32_t mqtt_file_size;
    // char mqtt_etx_tem[8];
    // char mqtt_etx_hum[8];
    // char mqtt_DS18B20_TEM[8];

} mqtt_json_s;

struct
{
    char wifi_ssid[32];
    char wifi_pwd[64];
} wifi_data;

struct
{
    char http_time[24];
} http_json_c;

typedef struct
{
    char buff[256];
    uint8_t len;
} creat_json;

//creat_json *create_http_json(uint8_t post_status);
void create_http_json(creat_json *pCreat_json, uint8_t flag);
void Read_Metadate_E2p(void);
void Read_Product_E2p(void);
void Read_Fields_E2p(void);
void Create_NET_Json(void);
void Create_Switch_Json(void);
uint16_t Create_Status_Json(char *status_buff, bool filed_flag);
char *mid(char *src, char *s1, char *s2, char *sub);
char *s_rstrstr(const char *_pBegin, int _MaxLen, int _ReadLen, const char *_szKey);
char *s_strstr(const char *_pBegin, int _ReadLen, int *first_len, const char *_szKey);

/************metadata 参数***********/
extern uint32_t fn_dp;      //数据发送频率
extern uint32_t fn_485_t;   //485 温度探头
extern uint32_t fn_485_th;  //485温湿度探头
extern uint32_t fn_485_sth; //485 土壤探头
extern uint32_t fn_485_ws;  //485 风速
extern uint32_t fn_485_lt;  //485 光照
extern uint32_t fn_485_co2; //485二氧化碳
extern uint32_t fn_ext;     //18b20
extern uint32_t fn_sw_e;    //电能信息：电压/电流/功率
extern uint32_t fn_sw_pc;   //用电量统计
extern uint8_t cg_data_led; //发送数据 LED状态 0关闭，1打开
extern uint8_t net_mode;    //上网模式选择 0：自动模式 1：lan模式 2：wifi模式
extern uint8_t de_sw_s;     //开关默认上电状态

/*********field num 相关参数************/
extern uint8_t sw_s_f_num;     //开关状态
extern uint8_t sw_v_f_num;     //插座电压
extern uint8_t sw_c_f_num;     //插座电流
extern uint8_t sw_p_f_num;     //插座功率
extern uint8_t sw_pc_f_num;    //累计用电量
extern uint8_t rssi_w_f_num;   //wifi信号
extern uint8_t rssi_g_f_num;   //4G信号
extern uint8_t r1_light_f_num; //485光照
extern uint8_t r1_th_t_f_num;  //485空气温度
extern uint8_t r1_th_h_f_num;  //485空气湿度
extern uint8_t r1_sth_t_f_num; //485土壤温度
extern uint8_t r1_sth_h_f_num; //485土壤湿度
extern uint8_t e1_t_f_num;     //DS18B20温度
extern uint8_t r1_t_f_num;     //485温度探头温度
extern uint8_t r1_ws_f_num;    //485风速
extern uint8_t r1_co2_f_num;   //485 CO2
extern uint8_t r1_ph_f_num;    //485 PH
extern uint8_t r1_co2_t_f_num; // CO2 温度
extern uint8_t r1_co2_h_f_num; //CO2 湿度

extern char SerialNum[SERISE_NUM_LEN];
extern char ProductId[PRODUCT_ID_LEN];
extern char ApiKey[API_KEY_LEN];
extern char ChannelId[CHANNEL_ID_LEN];
extern char USER_ID[USER_ID_LEN];
extern char WEB_SERVER[WEB_HOST_LEN];
extern char BleName[17];
extern char SIM_APN[32];
extern char SIM_USER[32];
extern char SIM_PWD[32];

#endif