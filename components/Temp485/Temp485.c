#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "Temp485.h"
#include "Jdq.h"
#include "sht31.h"

#define UART2_TXD  (GPIO_NUM_17)
#define UART2_RXD  (GPIO_NUM_16)
#define UART2_RTS  (UART_PIN_NO_CHANGE)
#define UART2_CTS  (UART_PIN_NO_CHANGE)

#define RS485RD    (GPIO_NUM_4)
#define BUF_SIZE    100

#define Ctl_Max_Temp    10      //控制阈值最高温度
#define Ctl_Min_Temp    5      //控制阈值最低温度

#define Alarm_Max_Temp    (Ctl_Max_Temp+5)      //报警阈值最高温度
#define Alarm_Min_Temp    (Ctl_Min_Temp-5)      //报警阈值最低温度

const char temp485_modbus_send_data[]={0x01,0x04,0x04,0x00,0x00,0x03,0xB1,0x3B};//发送查询温度指令3通道


/*******************************************************************************
// CRC16_Modbus Check
*******************************************************************************/
static uint16_t CRC16_ModBus(uint8_t *buf,uint8_t buf_len)  
{  
  uint8_t i,j;
  uint8_t c_val;
  uint16_t crc16_val = 0xffff;
  uint16_t crc16_poly = 0xa001;  //0x8005
  
  for (i = 0; i < buf_len; i++)
  {
    crc16_val = *buf^crc16_val;
    
    for(j=0;j<8;j++)
    {
      c_val = crc16_val&0x01;
      
      crc16_val = crc16_val >> 1;
      
      if(c_val)
      {
        crc16_val = crc16_val^crc16_poly;
      }
    }
    buf++;
  }
  //printf("crc16_val=%x\n",crc16_val);
  return ((crc16_val&0x00ff)<<8)|((crc16_val&0xff00)>>8);
}

int Temp485_Read(double *temp1,double *temp2,double* temp3)
{
    int ret;
    int len2=0;
    uint8_t data_u2[BUF_SIZE];
    gpio_set_level(RS485RD, 1); //RS485输出模式
    uart_write_bytes(UART_NUM_2, temp485_modbus_send_data, 8);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    gpio_set_level(RS485RD, 0); //RS485输入模式
    len2 = uart_read_bytes(UART_NUM_2, data_u2, BUF_SIZE, 20 / portTICK_RATE_MS);
    if(len2!=0)
    {
        printf("UART2 recv:");
        for(int i=0;i<len2;i++)
        {
            printf("%x ",data_u2[i]);
        }
        printf("\r\n");

        //crc_check=CRC16_ModBus(data_u2,5);
        //printf("crc-check=%d\n",crc_check);
        //printf("crc-check2=%d\n",(data_u2[5]*256+data_u2[6]));
        //printf("u25=%x,u26=%x\n",data_u2[5],data_u2[6]);
        if((data_u2[9]*256+data_u2[10])==CRC16_ModBus(data_u2,9))   //CRC校验成功,读取温度数据
        {
            uint16_t Temp1=((uint16_t)data_u2[3]<<8)+data_u2[4];
            uint16_t Temp2=((uint16_t)data_u2[5]<<8)+data_u2[6];
            uint16_t Temp3=((uint16_t)data_u2[7]<<8)+data_u2[8];

            if((Temp1|0x7fff)==0x7fff)//最高为=0，原码，温度正
            {
                *temp1=(double)Temp1/10;
            }
            else//最高为=1，补码，温度负
            {
                *temp1=(double)(Temp1-0xffff-0x1)/10;
            }
            
            if((Temp2|0x7fff)==0x7fff)//最高为=0，原码，温度正
            {
                *temp2=(double)Temp2/10;
            }
            else//最高为=1，补码，温度负
            {
                *temp2=(double)(Temp2-0xffff-0x1)/10;
            }

            if((Temp3|0x7fff)==0x7fff)//最高为=0，原码，温度正
            {
                *temp3=(double)Temp3/10;
            }
            else//最高为=1，补码，温度负
            {
                *temp3=(double)(Temp3-0xffff-0x1)/10;
            }
            ret=1;
        }
        else
        {
            ret=-1;
            printf("temp485 crc error\n");
        }

        len2=0;
        bzero(data_u2,sizeof(data_u2));
    }
    else//未接传感器
    {
        ret=0;
    }
    return ret;
}



void Temp485_Init(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = 1 << UART2_RXD;
    io_conf.mode = GPIO_MODE_INPUT;

    gpio_config(&io_conf);
    /**********************uart init**********************************************/
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    gpio_set_pull_mode(UART2_RXD, 0);
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, UART2_TXD, UART2_RXD, UART2_RTS, UART2_CTS);
    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0);

    
    /******************************gpio init*******************************************/
    //gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO16
    io_conf.pin_bit_mask = (1<<RS485RD);
    //disable pull-down mode
    io_conf.pull_down_en = 1;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    Pipeline_Temp_Channel1=0x7fff;
    Pipeline_Temp_Channel2=0x7fff;
    Pipeline_Temp_Channel3=0x7fff;

    Temp485_Read(&Pipeline_Temp_Channel1,&Pipeline_Temp_Channel2,&Pipeline_Temp_Channel3);
    printf("temp1=%.2f,temp2=%.2f,temp3=%.2f\r\n",Pipeline_Temp_Channel1,Pipeline_Temp_Channel2,Pipeline_Temp_Channel3);

}


void Temp_Ctl_App(void)//读取各传感器状态，并控制继电器开关，实现管道温度控制
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
        read485_errcount=0;
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
        read485_errcount++;
        if(read485_errcount>2)
        {
            read485_errcount=3;
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
    }

    if(strlen(ErrorCode)==0)
    {
        strcat(ErrorCode,"200");
        Jdq_Beep_Off();
    }

    printf("ErrorCode=%s\r\n",ErrorCode);


}