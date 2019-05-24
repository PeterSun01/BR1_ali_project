#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"


#include "mqtt_client.h"
#include "Mqtt.h"
#include "Json_parse.h"
#include "Smartconfig.h"
#include "E2prom.h"
#include "Led.h"
#include "libGSM.h"
#include "hmac_sha1.h"
#include "sht31.h"
#include "Temp485.h"
#include "Jdq.h"

#define Ctl_Max_Temp    33      //控制阈值最高温度
#define Ctl_Min_Temp    26      //控制阈值最低温度

#define Alarm_Max_Temp    (60)      //报警阈值最高温度
#define Alarm_Min_Temp    (Ctl_Min_Temp-5)      //报警阈值最低温度

#define MQTT_JSON_TYPE  0X03

static const char *TAG = "MQTT";
extern const int PPP_CONNECTED_BIT;

esp_mqtt_client_handle_t client;
EventGroupHandle_t mqtt_event_group;
static const int MQTT_CONNECTED_BIT = BIT0;

char Topic_Set[256];
char Topic_Post[256];

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;

    switch (event->event_id)
    {
        case MQTT_EVENT_BEFORE_CONNECT:
            break;
            
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

            xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
           
            msg_id = esp_mqtt_client_subscribe(client, Topic_Set, 0);//订阅
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_BIT);
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
            ESP_LOGW(TAG, "MQTT_EVENT_DATA=%s",event->data);
            //ESP_LOGW(TAG, "MQTT_EVENT_user_context=%s",event->user_context);
            ESP_LOGW(TAG, "MQTT_EVENT_topic=%s",event->topic);            

            //parse_objects_mqtt(event->data);//收到平台MQTT数据并解析
            bzero(event->data,strlen(event->data));
            //Mqtt_Send_Msg(Topic_Post);
            
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
    }
    return ESP_OK;
}

void Mqtt_Send_Msg(char* topic)
{
    int msg_id;
    creat_json *pCreat_json=malloc(sizeof(creat_json));
    create_mqtt_json(pCreat_json);

    msg_id = esp_mqtt_client_publish(client, topic, pCreat_json->creat_json_b, pCreat_json->creat_json_c, 0, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    Led_Status=LED_STA_SENDDATA;

    free(pCreat_json);
}

void Jdq_Ctl_App(void)//读取各传感器状态，并控制继电器
{
    ErrorStatus=0;
    bzero(ErrorCode,sizeof(ErrorCode));
    if(sht31_readTempHum()==0) //箱内温湿度读取错误
    {
        Humidity=0x7fff;
        Temperature=0x7fff;
        ErrorStatus=1;
        strcat(ErrorCode,"404;");
        
    }
    if(Temp485_Read(&Pipeline_Temp_Channel1,&Pipeline_Temp_Channel2,&Pipeline_Temp_Channel3)==1) //外接温度传感器有响应
    {
        //外接温度传感器全部故障
        if((Pipeline_Temp_Channel1==0x7fff)&&(Pipeline_Temp_Channel2==0x7fff)&&(Pipeline_Temp_Channel3==0x7fff))
        {
            ErrorStatus=1;
            strcat(ErrorCode,"401;402;403;");
            Jdq_Br_On();
            Jdq_Beep_On();
            //一直开启加热
            printf("Pipeline_Temp_Channel1-3 0x7fff\r\n");
        }
        else//外接传感器没有全部故障
        {
            if(Pipeline_Temp_Channel1==0x7fff) //外接1故障
            {
                ErrorStatus=1;
                strcat(ErrorCode,"401;");
            }
            if(Pipeline_Temp_Channel2==0x7fff) //外接2故障
            {
                ErrorStatus=1;
                strcat(ErrorCode,"402;");
            }
            if(Pipeline_Temp_Channel3==0x7fff) //外接3故障
            {
                ErrorStatus=1;
                strcat(ErrorCode,"403;");
            }

            //有一个管道温度<阈值最低温度，则开启加热
            if((Pipeline_Temp_Channel1<Ctl_Min_Temp)||(Pipeline_Temp_Channel2<Ctl_Min_Temp)||(Pipeline_Temp_Channel3<Ctl_Min_Temp))
            {
                Jdq_Br_On();
            }
            //三个管道温度全部>阈值最高温度，则关闭加热
            else if((Pipeline_Temp_Channel1>Ctl_Max_Temp)&&(Pipeline_Temp_Channel2>Ctl_Max_Temp)&&(Pipeline_Temp_Channel3>Ctl_Max_Temp))
            {
                Jdq_Br_Off();
            }

            //三个外接温度均小于报警最低温度阈值
            if((Pipeline_Temp_Channel1<Alarm_Min_Temp)&&(Pipeline_Temp_Channel2<Alarm_Min_Temp)&&(Pipeline_Temp_Channel3<Alarm_Min_Temp))
            {
                ErrorStatus=1;
                strcat(ErrorCode,"412;");
                Jdq_Br_On();
                Jdq_Beep_On();
            }

            //三个外接温度均高于报警最高温度阈值
            if((Pipeline_Temp_Channel1>Alarm_Max_Temp)&&(Pipeline_Temp_Channel2>Alarm_Max_Temp)&&(Pipeline_Temp_Channel3>Alarm_Max_Temp))
            {
                ErrorStatus=1;
                strcat(ErrorCode,"411;");
                Jdq_Br_Off();
                Jdq_Beep_On();
            }

        }
    }    
    else//外接温度传感器全部故障（断连、无响应）
    {
        printf("Pipeline_Temp not connect\r\n");
        Pipeline_Temp_Channel1=0x7fff;
        Pipeline_Temp_Channel2=0x7fff;
        Pipeline_Temp_Channel3=0x7fff;
        ErrorStatus=1;
        strcat(ErrorCode,"401;402;403;");
        Jdq_Br_On();
        Jdq_Beep_On();
        //一直开启加热
    }



    if(strlen(ErrorCode)==0)
    {
        strcat(ErrorCode,"200");
        Jdq_Beep_Off();
    }

    printf("ErrorCode=%s\r\n",ErrorCode);


}


static void MqttSend_Task(void* arg)
{
    while(1)
    {
        xEventGroupWaitBits(ppp_event_group, PPP_CONNECTED_BIT , false, true, portMAX_DELAY); 
        xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_BIT , false, true, portMAX_DELAY); 
        Jdq_Ctl_App();
        Mqtt_Send_Msg(Topic_Post);
        vTaskDelay(5000 / portTICK_RATE_MS);
    }
    
}

void initialise_mqtt(void)
{
    char mqtt_url[100]="\0";
    char mqtt_usrname[100]="\0";
    char mqtt_clientid[100]="\0";
    char mqtt_content[100]="\0";
    char mqtt_passwd[100]="\0";

    sprintf(mqtt_url,"mqtt://mqtt.iot-as-mqtt.cn-shanghai.aliyuncs.com");
    printf("mqtt_url=%s\n",mqtt_url);

    sprintf(mqtt_usrname,"%s&%s",DeviceName,ProductKey);
    printf("mqtt_usrname=%s\n",mqtt_usrname);

    sprintf(mqtt_clientid,"%s|securemode=3,signmethod=hmacsha1|",DeviceName);
    printf("mqtt_clientid=%s\n",mqtt_clientid);

    sprintf(mqtt_content,"clientId%sdeviceName%sproductKey%s",DeviceName,DeviceName,ProductKey);
    printf("mqtt_content=%s\n",mqtt_content);
    printf("DeviceSecret=%s\n",DeviceSecret);
    aliyun_iot_common_hmac_sha1(mqtt_content, strlen(mqtt_content), mqtt_passwd, DeviceSecret, strlen(DeviceSecret));
    printf("mqtt_passwd=%s\n",mqtt_passwd);

    //Topic_Set = "/sys/ProductKey/DeviceName/thing/service/property/set"           
    sprintf(Topic_Set,"/sys/%s/%s/thing/service/property/set",ProductKey,DeviceName);
    printf("Topic_Set=%s\n",Topic_Set);
    //Topic_Post=/sys/ProductKey/DeviceName/thing/event/property/post
    sprintf(Topic_Post,"/sys/%s/%s/thing/event/property/post",ProductKey,DeviceName);
    printf("Topic_Post=%s\n",Topic_Post);

    const esp_mqtt_client_config_t mqtt_cfg = {

        .uri = mqtt_url,
        .port = 1883,
        .event_handle = mqtt_event_handler,

        .username = mqtt_usrname,
        
        .password = mqtt_passwd,

        .client_id = mqtt_clientid,

        .keepalive = 300
    };

    mqtt_event_group = xEventGroupCreate();

    xEventGroupWaitBits(ppp_event_group, PPP_CONNECTED_BIT , false, true, portMAX_DELAY);
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
    xTaskCreate(MqttSend_Task, "MqttSend_Task", 8192, NULL, 4, NULL);
}
