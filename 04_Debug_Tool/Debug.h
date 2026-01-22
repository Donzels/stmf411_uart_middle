/******************************************************************************
 * Copyright (C) 2024 EternalChip, Inc.(Gmbh) or its affiliates.
 * 
 * All Rights Reserved.
 * 
 * @file Debug.h
 * 
 * @par dependencies 
 * - Debug.h
 * 
 * @author Simon | R&D Dept. | EternalChip 立芯嵌入式
 * 
 * @brief Provide all the debugging tools in this project.
 * 
 * Processing flow:
 * 
 * call directly.
 * 
 * @version V1.0 2025-2-24
 *
 * @note 1 tab == 4 spaces!
 * 
 *****************************************************************************/
#ifndef __DEBUG_H__  //Avoid repeated including same files later
#define __DEBUG_H__

//******************************** Includes *********************************//
#include "elog.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define DEBUG
#define OS_SUPPORTING


//所有trace输出均要经过当前模块控制输出
#ifdef DEBUG
/*trace输出Api*/
#define DEBUG_OUT(format, ...)          log_i(format, ##__VA_ARGS__)     
#define DEBUG_OUT_ERR(format, ...)      log_e(format, ##__VA_ARGS__) 

#define DEBUG_USER_APP

#define DEBUG_USART_CORE

#define DEBUG_UART_SEND
#define DEBUG_UART_PROTO

#endif
//******************************** Defines **********************************//

//******************************** Declaring ********************************//
void Debug_Init(void);                                                       
//******************************** Declaring ********************************//

#endif /* __EC_BSP_AHT21_DRIVER_H__ */
