
#ifndef __TCP_BSP_H__
#define __TCP_BSP_H__

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif
//#define TCP_SERVER_CLIENT_OPTION FALSE              //esp32作为client
#define TCP_SERVER_CLIENT_OPTION TRUE //esp32作为server

#define TAG "User_Wifi" //打印的tag

#define TCP_SERVER_ADRESS "192.168.169.212" //作为client，要连接TCP服务器地址
#define TCP_PORT 5001                       //统一的端口号，包括TCP客户端或者服务端

extern int g_total_data;
extern bool g_rxtx_need_restart;

void my_tcp_connect(void);
void my_tcp_connect_task(void *pvParameters);
void tcp_send_buff(char *send_buff, uint16_t buff_len);

//create a tcp server socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_tcp_server(bool isCreatServer);
//create a tcp client socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_tcp_client();

// //send data task
// void send_data(void *pvParameters);
//receive data task
void recv_data(void *pvParameters);

//close all socket
void close_socket();

//get socket error code. return: error code
int get_socket_error_code(int socket);

//show socket error code. return: error code
int show_socket_error_reason(const char *str, int socket);

//check working socket
int check_working_socket();

#endif /*#ifndef __TCP_BSP_H__*/
