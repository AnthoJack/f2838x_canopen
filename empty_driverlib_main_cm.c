//#############################################################################
//
// FILE:   2838x_cm_canopen.c
//
// TITLE:  F2838x CM CANopen
//!
//! <h1> Base CAN project for iAi mandate </h1>
//!
//! This project is meant to provide a base for the implementation of a
//! CANopen stack
//
//#############################################################################
//
//
// $Copyright:
// Copyright (C) 2022 Texas Instruments Incorporated - http://www.ti.com
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions 
// are met:
// 
//   Redistributions of source code must retain the above copyright 
//   notice, this list of conditions and the following disclaimer.
// 
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the 
//   documentation and/or other materials provided with the   
//   distribution.
// 
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// $
//#############################################################################

//
// Included Files
//
#include "driverlib_cm.h"
#include "cm.h"
#include "CANopen.h"
#include "OD.h"

// Define macros and constants
#define BITRATE 500
#define BIT_TIME 16
#define IDENT 0x1
#define RX_MSG_OBJ 0x0
#define TX_MSG_OBJ 0x1
#define MAX_DLC 8
#define TIMER_PERIOD_TICKS 125000
#define NODE_ID 10
/* default values for CO_CANopenInit() */
#define NMT_CONTROL \
            CO_NMT_STARTUP_TO_OPERATIONAL \
          | CO_NMT_ERR_ON_ERR_REG \
          | CO_ERR_REG_GENERIC_ERR \
          | CO_ERR_REG_COMMUNICATION
#define FIRST_HB_TIME 500
#define SDO_SRV_TIMEOUT_TIME 1000
#define SDO_CLI_TIMEOUT_TIME 500
#define SDO_CLI_BLOCK false
#define OD_STATUS_BITS NULL

// Global variables
CO_t *CO = NULL;

// Thread (ISR) functions

/*__interrupt*/ void timerThread(void)
{
    bool_t syncWas;
    uint32_t timeDelta_us = 1000;

    if(!CO->CANmodule->CANnormal) return;

    syncWas = CO_process_SYNC(CO, timeDelta_us, NULL);
    CO_process_RPDO(CO, syncWas, timeDelta_us, NULL);
    CO_process_TPDO(CO, syncWas, timeDelta_us, NULL);
}

//
// Main
//
void main(void)
{
    CO_ReturnError_t err;
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
    uint32_t heapMemoryUsed;
    void *CANptr = (void*)CANA_BASE;
    uint32_t errInfo;

    // Configure microcontroller
    CM_init();

    // Setup CANopen
    CO = CO_new(NULL, &heapMemoryUsed);
    if(CO == NULL)
    {
        return;
    }

    SYSTICK_setPeriod(TIMER_PERIOD_TICKS);
    SYSTICK_registerInterruptHandler(timerThread);
    SYSTICK_enableInterrupt();
    Interrupt_enableInProcessor();

    while(reset != CO_RESET_APP)
    {
        CO->CANmodule->CANnormal = false;

        CO_CANsetConfigurationMode((void*) &CANptr);
        CO_CANmodule_disable(CO->CANmodule);

        err = CO_CANinit(CO, CANptr, BITRATE);
        if(err != CO_ERROR_NO)
        {
            return;
        }

        err = CO_CANopenInit(CO,
                            NULL,
                            NULL,
                            OD,
                            OD_STATUS_BITS,
                            NMT_CONTROL,
                            FIRST_HB_TIME,
                            SDO_SRV_TIMEOUT_TIME,
                            SDO_CLI_TIMEOUT_TIME,
                            SDO_CLI_BLOCK,
                            NODE_ID,
                            &errInfo);
        if(err != CO_ERROR_NO)
        {
            return;
        }
        err = CO_CANopenInitPDO(CO, CO->em, OD, NODE_ID, &errInfo);
        if(err != CO_ERROR_NO) {
            return;
        }

        CO_CANsetNormalMode(CO->CANmodule);

        SYSTICK_enableCounter();

        reset = CO_RESET_NOT;
        while(reset == CO_RESET_NOT)
        {
            uint32_t timeDelta_us = 500;

            reset = CO_process(CO, false, timeDelta_us, NULL);

        }

        SYSTICK_disableCounter();
    }

    SYSTICK_disableInterrupt();
    SYSTICK_unregisterInterruptHandler();

    CO_CANsetConfigurationMode(CO->CANmodule->CANptr);
    CO_delete(CO);

    // Stop
    __asm("    bkpt #0");
}

//
// End of File
//
