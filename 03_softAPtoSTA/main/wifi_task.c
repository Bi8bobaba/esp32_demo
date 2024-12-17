#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ap_http_server.h"
#include "wifi_task.h"

#include "lwip/err.h"
#include "lwip/sys.h"

/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/

#define WIFI_AP_SSID        "Testwifi"
#define WIFI_AP_PASS        "1234567890"
#define WIFI_AP_CHANNEL     1
#define WIFI_MAX_STA_CONN   4

/* esp netif object representing the WIFI station */
static esp_netif_t *sta_netif = NULL;

static const char *AP_TAG = "AP_info";
static const char *STA_TAG = "STA_info";

static void wifi_apinit(void);

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            esp_err_t ret = esp_wifi_connect();
            ESP_LOGI(STA_TAG, "sta_start is %d",ret);
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(STA_TAG, "connected");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(STA_TAG, "disconnected");
            esp_wifi_stop();
            wifi_apinit();  //sta模式切换失败，重新打开ap模式
            break;
        case WIFI_EVENT_AP_STACONNECTED:
        {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGI(AP_TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
        }break;
        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGI(AP_TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
        }break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
        {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(STA_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        }break;
        default:
            break;
        }
    }
    else
    {
        /*nothing*/
    }
}

static void wifi_apinit(void)
{
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASS,
            .max_connection = WIFI_MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    if (strlen(WIFI_AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(AP_TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL);
}

wifi_config_t wifi_staconfig;        //sta model configuration

static void wifi_stainit(void)
{
    
    ESP_LOGI(STA_TAG, "wifi_sta_init");
    memcpy(wifi_staconfig.sta.ssid,(uint8_t*)config_wifi.config_wifi_name,sizeof(wifi_staconfig.sta.ssid));
    memcpy(wifi_staconfig.sta.password,(uint8_t*)config_wifi.config_wifi_password,sizeof(wifi_staconfig.sta.password));
    wifi_staconfig.sta.scan_method = WIFI_FAST_SCAN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_staconfig));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    sta_netif = esp_netif_create_default_wifi_ap();
    assert(sta_netif);
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );  //wifi配置存储介质选择

    wifi_apinit();
}


void wifi_task(void *pvParameters)
{
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        if(config_wifi.config_useable == CONFIG_OK)
        {
            wifi_stainit();
            config_wifi.config_useable = CONFIG_NOK;   //防止重复配网
        }
    }
}

