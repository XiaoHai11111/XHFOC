#ifndef LOOP_H
#define LOOP_H

#ifdef __cplusplus
extern "C" {
#endif
/*---------------------------- C Scope ---------------------------*/
#include "stdint-gcc.h"

void Main();
void OnUartCmd(uint8_t* _data, uint16_t _len);

#ifdef __cplusplus
}
/*---------------------------- C++ Scope ---------------------------*/
#include <cstdio>
#include"stm32g4xx.h"
#endif
#endif
