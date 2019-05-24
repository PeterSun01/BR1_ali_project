#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include "esp_system.h"
#include "Json_parse.h"
#include "Nvs.h"
#include "esp_log.h"

#include "esp_wifi.h"
#include "Smartconfig.h"
#include "E2prom.h"
#include "sht31.h"
#include "Jdq.h"
#include "Temp485.h"
#include "Mqtt.h"


esp_err_t parse_Uart0(char *json_data)
{
    cJSON *json_data_parse = NULL;
    cJSON *json_data_parse_ProductKey = NULL;
    cJSON *json_data_parse_DeviceName = NULL;
    cJSON *json_data_parse_DeviceSecret = NULL;

    if(json_data[0]!='{')
    {
        printf("uart0 Json Formatting error1\n");
        return 0;
    }

    json_data_parse = cJSON_Parse(json_data);
    if (json_data_parse == NULL) //如果数据包不为JSON则退出
    {
        printf("uart0 Json Formatting error\n");
        cJSON_Delete(json_data_parse);

        return 0;
    }
    else
    {
        /*
        {
            "Command": "SetupALi",
            "ProductKey": "a18hJfuxArE",
            "DeviceName": "AIR2V001",
            "DeviceSecret": "kc0tfij2RbXdbOmiHSXnwmaZgR3CDE85",
            
        }
        */
        char zero_data[256];
        bzero(zero_data,sizeof(zero_data));
        
        json_data_parse_ProductKey = cJSON_GetObjectItem(json_data_parse, "ProductKey");
        if(json_data_parse_ProductKey!=NULL) 
        {
            E2prom_Write(PRODUCTKEY_ADDR, (uint8_t *)zero_data, PRODUCTKEY_LEN);            
            sprintf(ProductKey,"%s%c",json_data_parse_ProductKey->valuestring,'\0');
            E2prom_Write(PRODUCTKEY_ADDR, (uint8_t *)ProductKey, strlen(ProductKey));            
            printf("ProductKey= %s\n", json_data_parse_ProductKey->valuestring);
        }

        json_data_parse_DeviceName = cJSON_GetObjectItem(json_data_parse, "DeviceName"); 
        if(json_data_parse_DeviceName!=NULL) 
        {
            E2prom_Write(DEVICENAME_ADDR, (uint8_t *)zero_data, DEVICENAME_LEN);   
            sprintf(DeviceName,"%s%c",json_data_parse_DeviceName->valuestring,'\0');
            E2prom_Write(DEVICENAME_ADDR, (uint8_t *)DeviceName, strlen(DeviceName)); 
            printf("DeviceName= %s\n", json_data_parse_DeviceName->valuestring);
        }

        json_data_parse_DeviceSecret = cJSON_GetObjectItem(json_data_parse, "DeviceSecret"); 
        if(json_data_parse_DeviceSecret!=NULL) 
        {
            E2prom_Write(DEVICESECRET_ADDR, (uint8_t *)zero_data, DEVICESECRET_LEN);   
            sprintf(DeviceSecret,"%s%c",json_data_parse_DeviceSecret->valuestring,'\0');
            E2prom_Write(DEVICESECRET_ADDR, (uint8_t *)DeviceSecret, strlen(DeviceSecret));
            printf("DeviceSecret= %s\n", json_data_parse_DeviceSecret->valuestring);
        }  

        printf("{\"status\":\"success\",\"err_code\": 0}");
        cJSON_Delete(json_data_parse);
        fflush(stdout);//使stdout清空，就会立刻输出所有在缓冲区的内容。
        esp_restart();//芯片复位 函数位于esp_system.h
        return 1;

    }
}


/*
{
	"method": "thing.event.property.post",
	"params": {
		"Device_ID": "AA",
		"Internal_Temperature": 1,
		"Internal_Humidity": 2,
		"Pipeline_Temp_Channel1": 3,
		"Pipeline_Temp_Channel2": 4,
		"Pipeline_Temp_Channel3": 5,
		"Jdq_Br_Status": 0,
		"Jdq_Beep_Status": 0,
		"ErrorCode": "401;402",
 	    "ErrorStatus": 1,
         "GeoLocation": {
			"CoordinateSystem": 1,
			"Latitude": 39.12,
			"Longitude": 121.34,
			"Altitude": 12.5
		}
	}
}
*/


void create_mqtt_json(creat_json *pCreat_json)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *params = cJSON_CreateObject();
    

    cJSON_AddItemToObject(root, "method", cJSON_CreateString("thing.event.property.post"));
    cJSON_AddItemToObject(root, "params", params);

    cJSON_AddItemToObject(params, "Device_ID", cJSON_CreateString(DeviceName));

    cJSON_AddItemToObject(params, "Internal_Temperature", cJSON_CreateNumber(Temperature));
    cJSON_AddItemToObject(params, "Internal_Humidity", cJSON_CreateNumber(Humidity)); 
    ESP_LOGI("SHT30", "Temperature=%.1f, Humidity=%.1f", Temperature, Humidity);
	 

    cJSON_AddItemToObject(params, "Pipeline_Temp_Channel1", cJSON_CreateNumber(Pipeline_Temp_Channel1));
    cJSON_AddItemToObject(params, "Pipeline_Temp_Channel2", cJSON_CreateNumber(Pipeline_Temp_Channel2));
    cJSON_AddItemToObject(params, "Pipeline_Temp_Channel3", cJSON_CreateNumber(Pipeline_Temp_Channel3));
    printf("temp1=%.2f,temp2=%.2f,temp3=%.2f\r\n",Pipeline_Temp_Channel1,Pipeline_Temp_Channel2,Pipeline_Temp_Channel3);

    cJSON_AddItemToObject(params, "Jdq_Br_Status", cJSON_CreateNumber(Jdq_Br_Status));
    cJSON_AddItemToObject(params, "Jdq_Beep_Status", cJSON_CreateNumber(Jdq_Beep_Status));

    cJSON_AddItemToObject(params, "ErrorCode", cJSON_CreateString(ErrorCode));
    cJSON_AddItemToObject(params, "ErrorStatus", cJSON_CreateNumber(ErrorStatus));

    #define LON     121.505386
    #define LAT     38.840160
    #define ALT     149.12

    cJSON *GeoLocation = cJSON_CreateObject();
    cJSON_AddItemToObject(params, "GeoLocation", GeoLocation);
    cJSON_AddItemToObject(GeoLocation, "CoordinateSystem", cJSON_CreateNumber(1));
    cJSON_AddItemToObject(GeoLocation, "Latitude", cJSON_CreateNumber(LAT));
    cJSON_AddItemToObject(GeoLocation, "Longitude", cJSON_CreateNumber(LON));
    cJSON_AddItemToObject(GeoLocation, "Altitude", cJSON_CreateNumber(ALT));
  

    char *cjson_printunformat;
    cjson_printunformat=cJSON_PrintUnformatted(root);
    pCreat_json->creat_json_c=strlen(cjson_printunformat);
    bzero(pCreat_json->creat_json_b,sizeof(pCreat_json->creat_json_b));
    memcpy(pCreat_json->creat_json_b,cjson_printunformat,pCreat_json->creat_json_c);
    printf("len=%d,mqtt_json=%s\n",pCreat_json->creat_json_c,pCreat_json->creat_json_b);
    free(cjson_printunformat);
    cJSON_Delete(root);
    
}

