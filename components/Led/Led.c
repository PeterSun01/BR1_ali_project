#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "Led.h"

#define GPIO_LED_SYS    18
#define GPIO_LED_JDQ    5

static void Led_Task(void* arg)
{
    while(1)
    {
        switch(Led_Status)
        {
            case LED_STA_INIT:
                Led_SYS_On();
                vTaskDelay(10 / portTICK_RATE_MS);
                break;
                       
            case LED_STA_NOSER:
                Led_SYS_On();
                vTaskDelay(100 / portTICK_RATE_MS);
                Led_SYS_Off();
                vTaskDelay(100 / portTICK_RATE_MS);
                break;
            
            case LED_STA_WIFIERR:
                Led_SYS_On();
                vTaskDelay(300 / portTICK_RATE_MS);
                Led_SYS_Off();
                vTaskDelay(300 / portTICK_RATE_MS);
                break;

            case LED_STA_SENDDATA:
                Led_SYS_On();
                vTaskDelay(200 / portTICK_RATE_MS);
                Led_SYS_Off();
                Led_Status=LED_STA_SENDDATAOVER;
                break;

            case LED_STA_SENDDATAOVER:
                Led_SYS_Off();
                vTaskDelay(10 / portTICK_RATE_MS);
                break;
        }
        

    }
}

void Led_Init(void)
{
    gpio_config_t io_conf;

    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;

    io_conf.pin_bit_mask = (1ULL <<GPIO_LED_SYS);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 1;
    //configure GPIO with the given settings
    gpio_config(&io_conf);  

    io_conf.pin_bit_mask = (1ULL<<GPIO_LED_JDQ);
    gpio_config(&io_conf); 

    gpio_set_level(GPIO_LED_JDQ, 1);
    gpio_set_level(GPIO_LED_SYS, 1);

    Led_Status=LED_STA_INIT;

    xTaskCreate(Led_Task, "Led_Task", 4096, NULL, 5, NULL);

}


void Led_JDQ_On(void)
{
    gpio_set_level(GPIO_LED_JDQ, 0);
}

void Led_SYS_On(void)
{
    gpio_set_level(GPIO_LED_SYS, 0);
}

void Led_SYS_Off(void)
{
    gpio_set_level(GPIO_LED_SYS, 1);
}

void Led_JDQ_Off(void)
{
    gpio_set_level(GPIO_LED_JDQ, 1);
}


