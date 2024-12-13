#include "ap_http_server.h"
#include "esp_wifi.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <esp_log.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "cJSON.h"

#include "nvs_flash.h"
#include "esp_http_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_task_wdt.h"
#include "lwip/opt.h"

#include "lwip/lwip_napt.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/inet.h"
/*embed html*/
extern const char upload_script_start[] asm("_binary_wifi_html_start"); //wifi.html文件头
extern const char upload_script_end[]   asm("_binary_wifi_html_end");   //wifi.html文件尾

char wifi_name[30];
char wifi_password[30];

static const char *HTTP_TAG = "http_STA";


/* An HTTP GET handler */
static esp_err_t index_get_handler(httpd_req_t *req)
{
    /* Set some custom headers */
    httpd_resp_set_hdr(req, "server name", "Little");
    httpd_resp_set_hdr(req, "server size", "10G");

    // const char* resp_str = (const char*)req->user_ctx;
    // httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    const uint32_t upload_script_size = upload_script_end - upload_script_start;
    /* Add file upload form and script which on execution sends a POST request to /upload */
    httpd_resp_set_type(req,HTTPD_TYPE_TEXT);           //设置类型，http类型文本文件
    httpd_resp_send(req, upload_script_start, upload_script_size);
    return ESP_OK;
}

static const httpd_uri_t http_index = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "hello http"
};

static esp_err_t echo_post_handler(httpd_req_t *req)
{
    char buf[200];
    // char ssid[10];
    // char pswd[10];
    int ret, remaining = req->content_len;  //状态和剩余缓存

    while (remaining > 0) 
    {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        remaining)) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        /* Send back the same data */
        httpd_resp_send_chunk(req, buf, ret);       //构建组成http报文
        remaining -= ret;       //更新剩余缓存

        // receiveData是要剖析的数据
        //首先整体判断是否为一个json格式的数据
        cJSON *pJsonRoot = cJSON_Parse(buf);
        //如果是否json格式数据
        if (pJsonRoot !=NULL)
        {
            cJSON *pData = cJSON_GetObjectItem(pJsonRoot, "wifi_name");    // 解析mac字段字符串内容
            if(pData) {
                if (cJSON_IsString(pData))                           // 判断字段是否string类型
                {
                    strcpy(wifi_name, pData->valuestring);               // 拷贝内容到字符串数组
                    ESP_LOGI(HTTP_TAG, "wifi_name = %s\r\n",wifi_name);
                }
            }
            else {
                ESP_LOGI(HTTP_TAG, "error\r\n");
            }
            pData = cJSON_GetObjectItem(pJsonRoot, "wifi_code");    // 解析mac字段字符串内容
            if(pData) {
                if (cJSON_IsString(pData))                           // 判断字段是否string类型
                {
                    strcpy(wifi_password, pData->valuestring);               // 拷贝内容到字符串数组
                    ESP_LOGI(HTTP_TAG, "wifi_code = %s\r\n",wifi_password);
                }
            }
            else {
                ESP_LOGI(HTTP_TAG, "error\r\n");
            }
        }
        /* Log data received */
        ESP_LOGI(HTTP_TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(HTTP_TAG, "%.*s", ret,buf);
        ESP_LOGI(HTTP_TAG, "====================================");
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    // if(strcmp(wifi_name ,"\0")!=0 && strcmp(wifi_password,"\0")!=0)
    // {
    //     xSemaphoreGive(ap_sem);
    //     ESP_LOGI(HTTP_TAG, "set wifi name and password successfully! goto station mode");
    // }
    return ESP_OK;
}

static const httpd_uri_t http_echo = {
    .uri       = "/wifi_data",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
    .user_ctx  = NULL
};

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(HTTP_TAG, "Redirecting to root");
    return ESP_OK;
}

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.lru_purge_enable = true;
    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;
    // Start the httpd server
    ESP_LOGI(HTTP_TAG, "HTTP Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) 
    {
        // Set URI handlers
        ESP_LOGI(HTTP_TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &http_echo);
        httpd_register_uri_handler(server, &http_index);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
        #if CONFIG_EXAMPLE_BASIC_AUTH
        httpd_register_basic_auth(server);
        #endif
        return server;
    }

    ESP_LOGI(HTTP_TAG, "Error starting server!");
    return NULL;
}

