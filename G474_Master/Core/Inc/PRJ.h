/*
 * PRJ.h
 *
 *  Created on: Oct 28, 2023
 *      Author: Admin
 */

#ifndef INC_PRJ_H_
#define INC_PRJ_H_



#endif /* INC_PRJ_H_ */

#include "main.h"

typedef enum
{
  PRJ_OK       = 0x00U,
  PRJ_ERROR    = 0x01U,
  PRJ_BUSY     = 0x02U,
  PRJ_TIMEOUT  = 0x03U
} PRJ_StatusTypeDef;

PRJ_StatusTypeDef PRJ_PE_CAN_NEW_MODULE_INIT();

PRJ_StatusTypeDef PRJ_EV_TRANSMIT_CHARGING_START_CP();

PRJ_StatusTypeDef PRJ_EV_TRANSMIT_CHARGER_DATA();

PRJ_StatusTypeDef PRJ_EV_PROCESS_CHARGER_CONTROL_INFORMATION();

PRJ_StatusTypeDef PRJ_EV_VEHICLE_CONNECTOR_LOCK_CONFIRM();

PRJ_StatusTypeDef PRJ_EV_CONTACTOR_CHECK();

PRJ_StatusTypeDef PRJ_EV_POWERLINE_ISOLATION_TEST();

PRJ_StatusTypeDef PRJ_EV_ISOLATION_TEST_TERMINATE_CHECK();

PRJ_StatusTypeDef PRJ_HMI_UART_PROCESS_USER_SETTING();

PRJ_StatusTypeDef PRJ_EV_TRANSMIT_CHARGING_START_CP2();

PRJ_StatusTypeDef PRJ_PE_CAN_TRANSMIT_START_CHARGING();

PRJ_StatusTypeDef PRJ_PE_CAN_TRANSMIT_DATA_REQUEST();

PRJ_StatusTypeDef PRJ_PE_CAN_RECEIVE_DATA();

PRJ_StatusTypeDef PRJ_HMI_UART_TRANSMIT_DATA();

PRJ_StatusTypeDef PRJ_EV_RECEIVE_STOP_CHARGING();

PRJ_StatusTypeDef PRJ_PE_CAN_TRANSMIT_STOP_CHARGING();

PRJ_StatusTypeDef PRJ_HMI_UART_TRANSMIT_STOP_CHARGING();

PRJ_StatusTypeDef PRJ_EV_CHECK_DC_POWERLINE();

PRJ_StatusTypeDef PRJ_EV_UNLOCK_CONNECTOR();

PRJ_StatusTypeDef PRJ_EV_TERMINATE_COMMUNICATION();
