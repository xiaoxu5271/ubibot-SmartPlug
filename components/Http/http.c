#include "Http.h"
#include "nvs.h"
#include "Json_parse.h"
#include "E2prom.h"
#include "Bluetooth.h"
#include "Led.h"

#define WEB_SERVER "api.ubibot.cn"
#define WEB_PORT 80

extern const int CONNECTED_BIT;
extern uint8_t data_read[34];

static char *TAG = "HTTP";

uint8_t six_time_count = 4;

struct HTTP_STA
{
    char GET[10];
    char POST[10];
    char HEART_BEAT[64];

    char POST_URL1[64];
    char POST_URL_METADATA[16];
    char POST_URL_FIRMWARE[16];
    char POST_URL_SSID[16];
    char POST_URL_COMMAND_ID[16];

    char WEB_URL1[50];
    char WEB_URL2[20];
    char WEB_URL3[20];

    char HTTP_VERSION10[20];
    char HTTP_VERSION11[20];

    char HOST[30];
    char USER_AHENT[40];
    char CONTENT_LENGTH[30];
    char ENTER[10];
} http = {"GET ",
          "POST ",
          "http://api.ubibot.cn/heartbeat?api_key=",

          "http://api.ubibot.cn/update.json?api_key=",
          "&metadata=true",
          "&firmware=",
          "&ssid=",
          "&command_id=",

          "http://api.ubibot.cn/products/",
          "/devices/",
          "/activate",

          " HTTP/1.0\r\n",
          " HTTP/1.1\r\n",

          "Host: api.ubibot.cn\r\n",
          "User-Agent: dalian urban ILS1\r\n",
          "Content-Length:",
          "\r\n\r\n"};

TaskHandle_t httpHandle = NULL;
esp_timer_handle_t http_suspend_p = 0;

void http_suspends(void *arg)
{
    xTaskResumeFromISR(httpHandle);
}

esp_timer_create_args_t http_suspend = {
    .callback = &http_suspends,
    .arg = NULL,
    .name = "http_suspend"};

void http_get_task(void *pvParameters)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo *res;
    struct in_addr *addr;
    int32_t s = 0, r = 0;

    char recv_buf[1024];
    char build_heart_url[256];


    sprintf(build_heart_url, "%s%s%s%s%s%s%s", http.GET, http.HEART_BEAT, ApiKey, 
            http.HTTP_VERSION10, http.HOST, http.USER_AHENT, http.ENTER);


   
    /***打开定时器10s开启一次***/
    esp_timer_create(&http_suspend, &http_suspend_p);
    esp_timer_start_periodic(http_suspend_p, 1000 * 1000 * 10);
    /***打开定时器×**/

    while (1)
    {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.a
        */

        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");

             
        six_time_count++;//定时10s
        if (six_time_count >= 1)
        {
            six_time_count = 0;

            int err = getaddrinfo(WEB_SERVER, "80", &hints, &res);

            if (err != 0 || res == NULL)
            {
                ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                continue;
            }

            /* Code to print the resolved IP.
            Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
            addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
            ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));
            //ESP_LOGI("wifi", "1free Heap:%d,%d", esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_8BIT));

            s = socket(res->ai_family, res->ai_socktype, 0);
            if (s < 0)
            {
                ESP_LOGE(TAG, "... Failed to allocate socket.");
                freeaddrinfo(res);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                continue;
            }
            ESP_LOGI(TAG, "... allocated socket");

            //连接
            int http_con_ret;
            http_con_ret=connect(s, res->ai_addr, res->ai_addrlen);
            if ( http_con_ret != 0)
            {
                ESP_LOGE(TAG, "... socket connect failed1 errno=%d", errno);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                if ( http_con_ret != 0)
                {
                    ESP_LOGE(TAG, "... socket connect failed1 errno=%d", errno);
                    close(s);
                    freeaddrinfo(res);
                    //vTaskDelay(4000 / portTICK_PERIOD_MS);
                    //continue;
                    goto stop;
                }
            }
            ESP_LOGI(TAG, "... connected");
            freeaddrinfo(res);

            

            /**************发送心跳*******************************************************/
            if (write(s, build_heart_url, strlen(build_heart_url)) < 0)
            {
                ESP_LOGE(TAG, "... socket send failed");
                close(s);
                vTaskDelay(4000 / portTICK_PERIOD_MS);
                continue;
            }
            ESP_LOGI(TAG, "... heartbeat send success=\n%s",build_heart_url);
            

            struct timeval receiving_timeout;
            receiving_timeout.tv_sec = 5;
            receiving_timeout.tv_usec = 0;
            if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                           sizeof(receiving_timeout)) < 0)
            {
                ESP_LOGE(TAG, "... failed to set socket receiving timeout");
                close(s);
                vTaskDelay(4000 / portTICK_PERIOD_MS);
                continue;

            }
            ESP_LOGI(TAG, "... set socket receiving timeout success");
            

            /* Read HTTP response */
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf) - 1);
            printf("r=%d,hart_recv_data=%s\r\n", r,recv_buf);
            close(s);
            
            if(r>0)
            {
                parse_objects_heart(strchr(recv_buf, '{'));  
                http_send_mes();   
            } 
            else
            {
                 printf("hart recv 0!\r\n");
            }  
           
        } 
        stop:
        vTaskSuspend(httpHandle);
    }
}

//激活流程
int http_activate(void)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;

    int32_t s = 0, r = 0;

    char build_http[256];
    char recv_buf[1024];

    sprintf(build_http, "%s%s%s%s%s%s%s", http.GET, http.WEB_URL1, ProductId, http.WEB_URL2,SerialNum, http.WEB_URL3,http.ENTER);
                                                   //http.HTTP_VERSION10, http.HOST, http.USER_AHENT, http.ENTER);

    printf("build_http=%s\n",build_http);
    while(1)
    {
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        int err = getaddrinfo(WEB_SERVER, "80", &hints, &res);

        if (err != 0 || res == NULL)
        {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        /* Code to print the resolved IP.
        Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if (s < 0)
        {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket");

        if (connect(s, res->ai_addr, res->ai_addrlen) != 0)
        {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);
        if (write(s, build_http, strlen(build_http)) < 0)
        {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");
        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                        sizeof(receiving_timeout)) < 0)
        {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");

        /* Read HTTP response */

        bzero(recv_buf,sizeof(recv_buf));
        r = read(s, recv_buf, sizeof(recv_buf) - 1);
        printf("r=%d,activate recv_buf=%s\r\n",r, recv_buf);
        close(s);
             
        
        return parse_objects_http_active(strchr(recv_buf, '{'));
    }
}

void http_send_mes()  //上传传感器数据
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo *res;
    struct in_addr *addr;
    char recv_buf[1024];
    char build_po_url[256];
    char build_po_url_json[1024];
    int32_t s = 0, r = 0;
    creat_json *pCreat_json1=malloc(sizeof(creat_json));
 
    //创建POST的json格式
    create_http_json(pCreat_json1);


    sprintf(build_po_url, "%s%s%s%s%s%s%s%s%s%s%s%s%s%d%s", http.POST, http.POST_URL1, ApiKey, http.POST_URL_FIRMWARE,FIRMWARE,http.POST_URL_SSID,wifi_data.wifi_ssid,http.POST_URL_COMMAND_ID, mqtt_json_s.mqtt_command_id,
        http.HTTP_VERSION11, http.HOST, http.USER_AHENT, http.CONTENT_LENGTH, pCreat_json1->creat_json_c, http.ENTER);
    

    sprintf(build_po_url_json, "%s%s", build_po_url, pCreat_json1->creat_json_b);
    
    
    free(pCreat_json1);
    printf("POSTJSON=\r\n%s\r\n", build_po_url_json);
    //ESP_LOGI("wifi", "2free Heap:%d,%d", esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_8BIT));  
    
    int err = getaddrinfo(WEB_SERVER, "80", &hints, &res);
    if (err != 0 || res == NULL)
    {
        ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    /* Code to print the resolved IP.
           Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
    addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

    s = socket(res->ai_family, res->ai_socktype, 0);
    if (s < 0)
    {
        ESP_LOGE(TAG, "... Failed to allocate socket.");
        freeaddrinfo(res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "... allocated socket");

    //连接
    int http_con_ret;
    http_con_ret=connect(s, res->ai_addr, res->ai_addrlen);
    if ( http_con_ret != 0)
    {
        ESP_LOGE(TAG, "... socket connect failed1 errno=%d", errno);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if ( http_con_ret != 0)
        {
            ESP_LOGE(TAG, "... socket connect failed1 errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            //vTaskDelay(4000 / portTICK_PERIOD_MS);
            //continue;
            return;
        }
    }
    ESP_LOGI(TAG, "... connected");
    freeaddrinfo(res);

    
    //发送
    if (write(s, build_po_url_json, strlen(build_po_url_json)) < 0)
    {
        ESP_LOGE(TAG, "... socket send failed");
        close(s);
        vTaskDelay(4000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "... http socket send success");

    Led_Status=LED_STA_SENDDATA;

    //设置接收
    struct timeval receiving_timeout;


    receiving_timeout.tv_sec = 5;
    receiving_timeout.tv_usec = 0;
    
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                   sizeof(receiving_timeout)) < 0)
    {
        ESP_LOGE(TAG, "... failed to set socket receiving timeout");
        close(s);
        vTaskDelay(4000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "... set socket receiving timeout success");

    /* Read HTTP response */
    //接收HTTP返回
    bzero(recv_buf,sizeof(recv_buf));
    r = read(s, recv_buf, sizeof(recv_buf) - 1);
    printf("r=%d,recv=%s\r\n",r, recv_buf);
    close(s);

    //解析返回数据
    if(r>0)
    {
        parse_objects_http_respond(strchr(recv_buf, '{'));
    }
}

void initialise_http(void)
{
    xTaskCreate(&http_get_task, "http_get_task", 8192, NULL, 10, &httpHandle);
}