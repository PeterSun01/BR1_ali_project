#ifndef _JDQ_H_
#define _JDQ_H_

#include "freertos/FreeRTOS.h"

extern void Jdq_Init(void);
extern void Jdq_Br_On(void);
extern void Jdq_Br_Off(void);
extern void Jdq_Beep_On(void);
extern void Jdq_Beep_Off(void);

bool Jdq_Br_Status;
bool Jdq_Beep_Status;


#endif

