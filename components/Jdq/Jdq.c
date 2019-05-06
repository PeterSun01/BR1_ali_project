#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "Jdq.h"

#define GPIO_Jdq_Br       (GPIO_NUM_27)
#define GPIO_Jdq_Beep     (GPIO_NUM_26)


void Jdq_Init(void)
{
    gpio_config_t io_conf;

    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO16
    io_conf.pin_bit_mask = (1<<GPIO_Jdq_Br);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 1;
    //configure GPIO with the given settings
    gpio_config(&io_conf);  

    io_conf.pin_bit_mask = (1<<GPIO_Jdq_Beep);
    gpio_config(&io_conf); 

}


void Jdq_Br_On(void)
{
    gpio_set_level(GPIO_Jdq_Br, 1);
}

void Jdq_Br_Off(void)
{
    gpio_set_level(GPIO_Jdq_Br, 0);
}

void Jdq_Beep_On(void)
{
    gpio_set_level(GPIO_Jdq_Beep, 1);
}

void Jdq_Beep_Off(void)
{
    gpio_set_level(GPIO_Jdq_Beep, 0);
}


