#ifndef _MQTT_H_
#define _MQTT_H_

#include "freertos/FreeRTOS.h"

extern void initialise_mqtt(void);
void Mqtt_Send_Msg(char* topic);




#endif