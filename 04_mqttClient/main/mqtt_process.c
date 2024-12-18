#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_process.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
/*��ƽ̨��Ϣ*/
#define   CLOUD_HOST       "54bf80e940.st1.iotda-device.cn-east-3.myhuaweicloud.com" //���mqttHostUrl��Broker Address
#define   CLOUD_PORT       1883
#define   CLOUD_CLIENT_ID  "67564ef7bab900244b0dad74_dev_esp_0_0_2024121801"
#define   CLOUD_USERNAME   "67564ef7bab900244b0dad74_dev_esp"
#define   CLOUD_PASSWORD   "65d541ec5413dafaec206702024554a472f327344423284dfd0f89e1f101bf5d"

//��������:
#define SET_TOPIC  "$oc/devices/67564ef7bab900244b0dad74_dev_esp/sys/messages/down"//����
//��������:
#define POST_TOPIC "$oc/devices/67564ef7bab900244b0dad74_dev_esp/sys/properties/report"//����
//����ظ�����
#define CMD_RESPOND_TOPIC "$oc/devices/67564ef7bab900244b0dad74_dev_esp/sys/commands/response/%s"
//����ظ�����
#define CMD_RESPOND_DATA "{\"result_code\":0,\"response_name\":\"COMMAND_RESPONSE\",\"paras\":{\"result\":\"success\"}}"

static esp_mqtt_client_handle_t mqttclient;
static const char *TAG = "mqtt_info";
static uint8_t connectFg = false;       //ƽ̨���ӳɹ���־

void mqtt_data_process(esp_mqtt_event_handle_t mqttevent)
{
    //�·�ָ�������Ӧ��������(�����·�)
    if (strstr(mqttevent->topic, CLOUD_USERNAME) == NULL)
    {
        return;
    }
    char* p = NULL;
    char request_id[50];
    char request_data[512];
    int msg_id;

    p = strstr(mqttevent->topic, "request_id=");
    if (p)
    {
        //��������
        //$oc/devices/{device_id}/sys/commands/request_id={ request_id }
        //$oc/devices/{device_id}/sys/commands/request_id=1e1e1b54-b6d4-4c35-9cc0-e144c98efb5f
        strncpy(request_id, p, 47);
        request_id[47] = '\0';		//�������ַ����������β��'\0'����Ȼƽ̨��������
    }

    p = strstr(mqttevent->topic, "commands");   //����ָ��?
    if(p)
    {
        sprintf(request_data, CMD_RESPOND_TOPIC, request_id);
        msg_id = esp_mqtt_client_publish(mqttclient, (const char*)request_data, (const char*)CMD_RESPOND_DATA, strlen(CMD_RESPOND_DATA), 0, 0);
        ESP_LOGI(TAG, "commands respond successful, msg_id=%d", msg_id);
    }
}

/*�¼��ص�����*/
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:  //����ƽ̨�ɹ�
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        connectFg = true;
        //����������ƽ̨ get topic
        msg_id = esp_mqtt_client_subscribe(client, SET_TOPIC, 0);
        ESP_LOGI(TAG, "execute subscribe event, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");

        connectFg = false;
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");

        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        mqtt_data_process(event);
        break;
    case MQTT_EVENT_ERROR:
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

//test
double TEMP = 10.0;
#define DEV_SN "01020304"

/*mqtt ��������task����ʱ�ϱ�����*/
void mqtt_updata_process(void)
{
    if(!connectFg)
    {
        return;
    }
    int msg_id;
    char mqtt_updata[512] = {0};			//��Ϣ���ݻ�����
    sprintf(mqtt_updata, "{\"services\": [{\"service_id\": \"iot_mqtt_gateway\",\"properties\":{\"Param_temp\":%.1f,\"dev_SN\":\"%s\"}}]}", (double)(TEMP += 0.2),DEV_SN);//�ϱ�����
    //����Ϣ��������ƽ̨
    msg_id = esp_mqtt_client_publish(mqttclient, POST_TOPIC, (const char*)mqtt_updata, strlen(mqtt_updata), 0, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
}

/*mqtt ������*/
void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        // idf �°汾(esp-idf-V5.2.1����)������������
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
        .broker.address.hostname = CLOUD_HOST,
        .broker.address.port = CLOUD_PORT,
        .credentials.client_id = CLOUD_CLIENT_ID,
        .credentials.username = CLOUD_USERNAME,
        .credentials.authentication.password = CLOUD_PASSWORD,

    };

    mqttclient = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqttclient, ESP_EVENT_ANY_ID, mqtt_event_handler, mqttclient);
    esp_mqtt_client_start(mqttclient);
}
