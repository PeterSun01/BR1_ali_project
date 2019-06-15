#ifndef _TEMP485_H_
#define _TEMP485_H_

extern void Temp485_Init(void);
extern int Temp485_Read(double *temp1,double *temp2,double* temp3);
extern void Temp_Ctl_App(void);

double Pipeline_Temp_Channel1,Pipeline_Temp_Channel2,Pipeline_Temp_Channel3;

char ErrorCode[100];
bool ErrorStatus;
uint32_t read485_errcount;

#endif

