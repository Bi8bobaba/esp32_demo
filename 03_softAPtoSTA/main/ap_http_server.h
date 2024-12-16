#ifndef AP_HTTP_SERVER_H
#define AP_HTTP_SERVER_H

#include <esp_http_server.h>

#define CONFIG_BUFFER_SIZE                              (32)

enum{
    CONFIG_NOK  = (uint16_t)0x00,
    CONFIG_OK   = (uint16_t)0x01,
};

typedef struct
{
    uint16_t config_useable;
    char config_wifi_name[CONFIG_BUFFER_SIZE];
    char config_wifi_password[CONFIG_BUFFER_SIZE];
}config_wifiTPDF;

extern config_wifiTPDF config_wifi;

httpd_handle_t start_webserver(void);

#endif
