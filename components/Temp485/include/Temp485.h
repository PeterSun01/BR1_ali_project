#ifndef _TEMP485_H_
#define _TEMP485_H_

extern void Temp485_Init(void);
extern int Temp485_Read(double *temp1,double *temp2,double* temp3);

double Pipeline_Temp_Channel1,Pipeline_Temp_Channel2,Pipeline_Temp_Channel3;

#endif

